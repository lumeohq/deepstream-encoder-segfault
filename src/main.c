#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <gst/gst.h>

// Constants
#define TEST_SUCCESS_AFTER 5
#define TEST_TIMEOUT (TEST_SUCCESS_AFTER + 3)
#define DEFAULT_THREAD_COUNT 8
#define DEFAULT_ENCODERS 2


// Function to parse environment variables
int parse_env(const char* key, int default_value) {
    const char* value = getenv(key);
    if (value) {
        int result = atoi(value);
        printf("Configuration value parsed from env: %s=%d\n", key, result);
        return result;
    }
    printf("Using default value for %s: %d\n", key, default_value);
    return default_value;
}

// Function to get thread count from environment
int thread_count(void) {
    return parse_env("THREAD_COUNT", DEFAULT_THREAD_COUNT);
}

// Function to get encoder count from environment
int encoders(void) {
    return parse_env("ENCODERS_PER_PIPELINE", DEFAULT_ENCODERS);
}

// Function to create a pipeline string with multiple encoders
static char* create_pipeline_string(int num_encoders) {
    const char* pipeline_template = "videotestsrc ! nvvideoconvert ! nvv4l2h264enc ! fakesink ";
    const size_t template_len = strlen(pipeline_template);
    const size_t pipeline_size = template_len * num_encoders + 1; // +1 for null terminator
    
    char* pipeline_str = malloc(pipeline_size);
    if (!pipeline_str) {
        printf("Failed to allocate memory for pipeline string\n");
        return NULL;
    }

    // Initialize the string
    pipeline_str[0] = '\0';
    
    // Build the pipeline string by concatenating encoder pipelines
    for (int encoder_idx = 0; encoder_idx < num_encoders; encoder_idx++) {
        // Calculate current position and remaining space in the buffer
        size_t current_length = strlen(pipeline_str);
        size_t remaining_space = pipeline_size - current_length;
        
        // Append the encoder pipeline template
        if (snprintf(pipeline_str + current_length, remaining_space, "%s", pipeline_template) < 0) {
            printf("Failed to construct pipeline string\n");
            free(pipeline_str);
            return NULL;
        }
    }

    return pipeline_str;
}

int run_pipeline_to_completion(void) {
    int ok = 0;
    const int num_encoders = encoders();

    // Create pipeline string
    char* pipeline_str = create_pipeline_string(num_encoders);
    if (!pipeline_str) {
        return 0;
    }

    // Parse pipeline
    GError* error = NULL;
    GstElement* pipeline = gst_parse_launch(pipeline_str, &error);
    free(pipeline_str);
    pipeline_str = NULL;

    if (error) {
        printf("Error parsing pipeline: %s\n", error->message);
        g_error_free(error);
        error = NULL;
        return 0;
    }

    // Get bus
    GstBus* bus = gst_element_get_bus(pipeline);
    if (!bus) {
        printf("Could not get bus\n");
        goto cleanup;
    }

    // Set pipeline to playing
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        printf("Could not change state to Playing\n");
        goto cleanup;
    }

    // Wait for messages
    GstMessage* msg = NULL;
    while ((msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR))) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_STATE_CHANGED: {
                GstState old_state, new_state, pending;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending);
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                    printf("Pipeline switched to %s state\n", gst_element_state_get_name(new_state));
                    if (new_state == GST_STATE_PLAYING) {
                        ok = 1;
                        goto cleanup;
                    }
                }
                break;
            }
            case GST_MESSAGE_ERROR: {
                GError* err = NULL;
                gchar* debug_info = NULL;
                gst_message_parse_error(msg, &err, &debug_info);
                printf("Got GStreamer error: %s\n", err->message);
                g_error_free(err);
                g_free(debug_info);
                goto cleanup;
            }
            default:
                break;
        }
        gst_message_unref(msg);
        msg = NULL;
    }

cleanup:
    if (msg) {
        gst_message_unref(msg);
    }
    if (bus) {
        gst_object_unref(bus);
    }
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }

    printf("Running pipeline %s\n", ok ? "succeeded" : "failed");
    return ok;
}

void* run_pipeline_in_a_loop(void* arg) {
    int id = *(int*)arg;
    time_t start = time(NULL);
    
    for (int iteration = 1; ; iteration++) {
        printf("Thread %d, iteration %d\n", id, iteration);
        int result = run_pipeline_to_completion();
        time_t runtime = time(NULL) - start;
        const char * result_str = result ? "succeeded" : "failed";
        printf("Thread %d runtime: %ld seconds, Result: %s\n", id, runtime, result_str);

        if (runtime > TEST_SUCCESS_AFTER) {
            printf("Thread %d: Test succeeded\n", id);
            break;
        }
    }
    
    return NULL;
}

// Main function
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    int ret = 0;

    printf("Starting...\n");

    // Initialize GStreamer
    gst_init(NULL, NULL);

    // Create worker threads
    int thread_count_val = thread_count();
    pthread_t* threads = malloc(thread_count_val * sizeof(pthread_t));
    if (!threads) {
        printf("Failed to allocate memory for threads\n");
        ret = 1;
        goto cleanup;
    }

    int* thread_ids = malloc(thread_count_val * sizeof(int));
    if (!thread_ids) {
        printf("Failed to allocate memory for thread IDs\n");
        ret = 1;
        goto cleanup;
    }

    for (int i = 0; i < thread_count_val; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, run_pipeline_in_a_loop, &thread_ids[i]) != 0) {
            printf("Failed to create worker thread %d\n", i);
            ret = 1;
            goto cleanup;
        }
    }

    // Wait for test timeout
    sleep(TEST_TIMEOUT);

    // Check if all threads finished
    int all_threads_finished = 1;
    for (int i = 0; i < thread_count_val; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            all_threads_finished = 0;
            break;
        }
    }

    printf("Test succeeded, all threads finished: %d\n", all_threads_finished);

cleanup:
    if (threads) {
        free(threads);
    }
    if (thread_ids) {
        free(thread_ids);
    }

    return ret;
}
