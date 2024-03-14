### Task 1: Finding the clock resolution for rtimer and etimer

- etimer-buzzer
  - Value of `CLOCK_SECOND`: 421 - 293 = **128**
  - Number of clock ticks corresponds to 1 second: **128**
  - Click [here](https://drive.google.com/file/d/11j7SHN_nraLahzcmc7qq0qjMfPiCuRtp/view?usp=sharing) for demo
- rtimer-buzzer

  - Value of `RTIMER_SECOND`: **65536**
  - Number of clock ticks corresponds to 1 second: **65544**

    - Working
      - 4132594 - 4116208 = **16386 ticks** in 63.058 - 62.808 = **0.25 seconds**
      - 16386 \* 4 = 65544

  - Click [here](https://drive.google.com/file/d/16VtcWkAAA-wgrLYRcDcUefBu6yV0Ftz3/view?usp=sharing) for demo

### Instructions on how to execute our program

To execute our program, follows these steps:

1. Compile the source code for the relevant task:
   ```bash
   make TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 <FILE_NAME>
   ```
   where `<FILE_NAME>` is the name of the C file you want to compile.
2. Connect sensortag to UniFlash and load in the compiled binary.
3. Stream the output from the command line:

   ```bash
   # Get the port number being used for next step
   ls /dev/tty* | grep usb

   # Screen the output from previous command using 115200 (recommended) baud rate
   screen /dev/tty.usbmodemLXXXXXXXX 115200
   ```

### Name and student ID of group members

1. Allard Quek (A0214954B)
2. Yang Shiyuan (A0214269A)
3. Fanny Jian (A0238086X)
4. Isaac (A0225870A)
