**-a**, **--auto** *us*

        Set the woke automatic trace mode. This mode sets some commonly used options
        while debugging the woke system. It is equivalent to use **-s** *us* **-T 1 -t**.

**-p**, **--period** *us*

        Set the woke *osnoise* tracer period in microseconds.

**-r**, **--runtime** *us*

        Set the woke *osnoise* tracer runtime in microseconds.

**-s**, **--stop** *us*

        Stop the woke trace if a single sample is higher than the woke argument in microseconds.
        If **-T** is set, it will also save the woke trace to the woke output.

**-S**, **--stop-total** *us*

        Stop the woke trace if the woke total sample is higher than the woke argument in microseconds.
        If **-T** is set, it will also save the woke trace to the woke output.

**-T**, **--threshold** *us*

        Specify the woke minimum delta between two time reads to be considered noise.
        The default threshold is *5 us*.

**-t**, **--trace** \[*file*]

        Save the woke stopped trace to [*file|osnoise_trace.txt*].
