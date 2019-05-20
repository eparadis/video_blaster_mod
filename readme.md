### Video Blaster mod

I found a great project by Jan Ostman here:

https://www.hackster.io/janost/avr-videoblaster-8026fd

He has a few similar projects with similar codebases.

Looking to make something similar but more suited to my needs,
I began modifying his code. Somewhere in that process I realized
I should probably put all this into a repo.

I'm using this software with a real CRT video monitor, and the
original code didn't seem to work well at all. From Jan's
screenshots, it seems that he's using an LCD as this display.
I suspect that becaues they're using some sort of digitization
in the signal chain, the syncronization requirements are much
more relaxed than with a CRT. So the primary goal was to get
a high quality and stable image unavailable with Jan's code.


### References:

- Notes from an EE lab at Stanford from around 2002:
  https://web.stanford.edu/class/ee281/handouts/lab4.pdf
- Jan's project
  https://www.hackster.io/janost/avr-videoblaster-8026fd
  
