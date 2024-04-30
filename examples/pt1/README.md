## CS4222 Project Task 2 Compilation Steps

1. Create a folder in `contiki-ng/examples`. 
   - e.g. `contiki-ng/example/pt2`.
2. Move the `task_2_group_23_sender.c`,  `task_2_group_23_receiver.c` and `Makefile` into the above folder.
3. In your preferred terminal, run `make TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 task_2_group_23_sender task_2_group_23_receiver` for compilation.
4. Load the respective binary file to the corresponding sender and receiver tag. 
5. Boot up the sensor tags and observe the behaviour via the respective screen. 