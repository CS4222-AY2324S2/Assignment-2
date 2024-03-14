### Task 1: Finding the clock resolution for rtimer and etimer

- etimer-buzzer
  - Value of `CLOCK_SECOND`: 421 - 293 = **128**
  - Number of clock ticks corresponds to 1 second: 128
  - Click [here](https://drive.google.com/file/d/11j7SHN_nraLahzcmc7qq0qjMfPiCuRtp/view?usp=sharing) for demo
- rtimer-buzzer
  - Value of `RTIMER_SECOND`: 65536
  - 4132594 - 4116208 = **16386 ticks** in 63.058 - 62.808 = **0.25 seconds**
  - Number of clock ticks corresponds to 1 second: 16386 \* 4 = 65544
  - Click [here](https://drive.google.com/file/d/16VtcWkAAA-wgrLYRcDcUefBu6yV0Ftz3/view?usp=sharing) for demo
