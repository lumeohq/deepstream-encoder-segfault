# DeepStream SEGFAULT

This is a minimal reproducible example of the segfault issue with DeepStream.

To run once:

```
./run.sh
```

To run test in a loop with different combinations of threads/encoders:

```
sudo apt install python3-tabulate
./table.py
```

- image `nvcr.io/nvidia/deepstream:7.1-triton-multiarch`
- GPU: 3070ti
- Drivers affected:
  - 550.120
  - 570.144
- Drivers not affected
  - 535.183

The test looks like this:
- `run.sh` starts the process
    - the process creates THREAD_COUNT threads
    - each thread runs a loop which stops after 5 seconds
        - each loop iteration:
            - creates a pipeline with ENCODERS_PER_PIPELINE encoders
            - starts the pipeline
            - if started or failed, stop/unref the pipeline
            - continues iteration if 5 seconds not elapsed yet

Pipeline looks like this:

```
videotestsrc ! nvvideoconvert ! nvv4l2h264enc ! fakesink  videotestsrc ! nvvideoconvert ! nvv4l2h264enc ! fakesink
```

Note that the pipeline contains `videotestsrc ! nvvideoconvert ! nvv4l2h264enc ! fakesink` part twice. In this case, ENCODERS_PER_PIPELINE=2.

If the pipeline starts well, we stop it and continue iteration.
So each thread starts and stops the pipeline in a loop.
In 5 seconds, each tread may start/stop a hundred of pipelines.

Of course, the pipeline with 14 encoders won't run well because 3070ti only has 8 encoder sessions, but it shouldn't segfault in any case.
If the pipeline fails to start, a GStreamer error is caught, the pipeline is stopped/unreffed, and iteration continues.

If the program runs for 5 seconds, we consider it a successful run, stop the threads gracefully and terminate with exit code 0.
When it doesn't segfault, we count it as a success, even in presence of GStreamer errors.

If program crashes, we just capture the exit code 139.

table.py script prints a table to find out the probability of crashes depending on thread and encoder count. To run it, install `tabulate`: `sudo apt install python3-tabulate`.

Let's look at cell `Thr=7 / Enc=2` as an example. `{0: 11, 139: 57}` means that test returned
  - 0 exit code (success) 11 times
  - 139 exit code (segfault) 57 times

```
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |   Thr \ Enc | 1                | 2                | 3                | 4                | 5                | 6                | 7                | 8                | 9                | 10              | 11               | 12               |
 +=============+==================+==================+==================+==================+==================+==================+==================+==================+====================================+==================+==================+
 |           1 | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 69}          | {0: 9}          | {0: 69}          | {0: 69}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           2 | {0: 69}          | {0: 69}          | {0: 66, 139: 3}  | {0: 67, 139: 2}  | {0: 69}          | {0: 50, 139: 19} | {0: 61, 139: 8}  | {0: 59, 139: 10} | {0: 56, 139: 13} | {0: 0, 139: 9}  | {0: 52, 139: 17} | {0: 45, 139: 24} |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           3 | {0: 55, 139: 14} | {0: 67, 139: 2}  | {0: 51, 139: 18} | {0: 37, 139: 32} | {0: 26, 139: 43} | {0: 37, 139: 32} | {0: 31, 139: 38} | {0: 33, 139: 36} | {0: 33, 139: 36} | {0: 1, 139: 38} | {0: 30, 139: 39} | {0: 27, 139: 42} |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           4 | {0: 59, 139: 10} | {0: 63, 139: 6}  | {0: 28, 139: 41} | {0: 18, 139: 51} | {0: 21, 139: 48} | {0: 18, 139: 51} | {0: 13, 139: 56} | {0: 26, 139: 43} | {0: 23, 139: 46} | {0: 2, 139: 37} | {0: 26, 139: 43} | {0: 31, 139: 38} |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           5 | {0: 65, 139: 4}  | {0: 21, 139: 48} | {0: 12, 139: 57} | {0: 24, 139: 45} | {0: 16, 139: 53} | {0: 22, 139: 47} | {0: 21, 139: 48} | {0: 47, 139: 22} | {0: 41, 139: 28} | {0: 7, 139: 42} | {0: 56, 139: 13} | {0: 68, 139: 1}  |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           6 | {0: 66, 139: 3}  | {0: 16, 139: 53} | {0: 19, 139: 50} | {0: 18, 139: 51} | {0: 29, 139: 40} | {0: 32, 139: 37} | {0: 44, 139: 25} | {0: 42, 139: 27} | {0: 56, 139: 13} | {0: 8, 139: 1}  | {0: 65, 139: 3}  | {0: 68}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           7 | {0: 62, 139: 6}  | {0: 11, 139: 57} | {0: 25, 139: 43} | {0: 24, 139: 44} | {0: 33, 139: 35} | {0: 38, 139: 30} | {0: 46, 139: 22} | {0: 65, 139: 3}  | {0: 66, 139: 2}  | {0: 8}          | {0: 68}          | {0: 68}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           8 | {0: 68}          | {0: 7, 139: 61}  | {0: 22, 139: 46} | {0: 31, 139: 37} | {0: 40, 139: 28} | {0: 46, 139: 22} | {0: 68}          | {0: 66, 139: 2}  | {0: 68}          | {0: 8}          | {0: 68}          | {0: 68}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |           9 | {0: 46, 139: 22} | {0: 18, 139: 50} | {0: 34, 139: 34} | {0: 33, 139: 35} | {0: 41, 139: 27} | {0: 41, 139: 27} | {0: 67, 139: 1}  | {0: 68}          | {0: 68}          | {0: 8}          | {0: 68}          | {0: 68}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |          10 | {0: 53, 139: 15} | {0: 20, 139: 48} | {0: 29, 139: 39} | {0: 48, 139: 20} | {0: 46, 139: 22} | {0: 67, 139: 1}  | {0: 68}          | {0: 67, 139: 1}  | {0: 68}          | {0: 8}          | {0: 68}          | {0: 68}          |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |          11 | {0: 43, 139: 25} | {0: 21, 139: 47} | {0: 39, 139: 29} | {0: 43, 139: 25} | {0: 60, 139: 8}  | {0: 68}          | {0: 68}          | {0: 68}          | {0: 68}          | {0: 8}          | {0: 68}          | {0: 67, 139: 1}  |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------------------------+------------------+------------------+
 |          12 | {0: 44, 139: 24} | {0: 41, 139: 27} | {0: 42, 139: 26} | {0: 48, 139: 20} | {0: 65, 139: 3}  | {0: 68}          | {0: 68}          | {0: 68}          | {0: 68}          | {0: 8}          | {0: 67, 139: 1}  | {0: 66, 139: 2}  |
 +-------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+
```

If running the program under gdb, the stacktrace leads to `libnvcuvid.so`

```
...
Running pipeline succeeded
Thread 7 runtime: 1 seconds, Result: succeeded
Thread 7, iteration 3
Configuration value parsed from env: ENCODERS_PER_PIPELINE=2
Pipeline switched to READY state
Pipeline switched to READY state
Running pipeline failed
Thread 5 runtime: 1 seconds, Result: failed
Thread 5, iteration 4
Configuration value parsed from env: ENCODERS_PER_PIPELINE=2
Pipeline switched to READY state
Failed to query video capabilities: Invalid argument
Failed to query video capabilities: Invalid argument
Failed to query video capabilities: Invalid argument
Pipeline switched to READY state
Failed to query video capabilities: Invalid argument
Pipeline switched to READY state
Failed to query video capabilities: Invalid argument
Pipeline switched to READY state
[New Thread 0x732e0f400640 (LWP 239)]
[New Thread 0x732e24e00640 (LWP 240)]

Thread 32 "videotestsrc2:s" received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x732e31000640 (LWP 222)]
0x0000732e71417941 in ?? () from /usr/lib/x86_64-linux-gnu/libnvcuvid.so.1
(gdb) bt
#0  0x0000732e71417941 in  () at /usr/lib/x86_64-linux-gnu/libnvcuvid.so.1
#1  0x0000732e71417a7a in  () at /usr/lib/x86_64-linux-gnu/libnvcuvid.so.1
#2  0x0000732e7148d326 in  () at /usr/lib/x86_64-linux-gnu/libnvcuvid.so.1
#3  0x0000732e7148da8d in  () at /usr/lib/x86_64-linux-gnu/libnvcuvid.so.1
#4  0x0000732e9d285ac3 in  () at /usr/lib/x86_64-linux-gnu/libc.so.6
#5  0x0000732e9d316a04 in clone () at /usr/lib/x86_64-linux-gnu/libc.so.6
```
