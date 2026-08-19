# Acces controll system

This project was carried out as part of an internship at Uniza

You can find the documentation of the project and the schematics in the `doc` folder

/!\ Don't forget to edit the `my_door_id` variable in the *main.c* file. It as to be an unique identifier for every single doors.

## Interact with CAN with you computer

Plug the peak usb and use PCAN View software.
If you don't want to use this software you can also use commandline tools.

`sudo apt-get install can-utils`

### create the can listener

*Plug the PEAK usb before ussing these commands*

`sudo ip link set can0 type can bitrate 500000`

`sudo ip link set can0 up`

We use the 500 kb/s bit rate for this project.

And for print this linstener in your terminal use `candump can0`

### send can messages

To send can messages with your computer, you can use PCAN View or the command lines.
Use the command *cansend* like that 
`cansend can0 123#42`

123 is the ID of CAN and 42 is the payload.

`cansend can0 123#41` --> REQ_DOOR

`cansend can0 123#42` --> REQ_OK

`cansend can0 123#43` --> REQ_NOT_OK

## Server side simulation

You can simulate the server side with your pc by using the python script

Don't forget to install the python-can library with `pip install python-can`

Then run the script with `python3 server_simulation.py` and don't forget to initialize the can interface 

example of sample request to open the door:
`cansend can0 123#0101020304050607`

