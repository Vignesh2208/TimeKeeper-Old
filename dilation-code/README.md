#TimeKeeper
```
TimeKeeper has been tested with Ubuntu 12.04 32-bit and 64-bit with Linux kernel version 3.10.9

Outlined below are basic instructions. See the installation/usage guide found in the documentation directory for additional assistance.
```

## TimeKeeper configuration
```
1. Setup Kernel:
	make setup_kernel
	
	#The kernel_setup script will download Linux Kernel version 3.13.1, and store it in /src directory. 
	#Then it will modify the source code with the necessary changes.
	#Compile the kernel. Follow the instructions outlined in: 
	#http://mitchtech.net/compile-linux-kernel-on-ubuntu-12-04-lts-detailed/.


2. Build TimeKeeper:
	make build
	
	#Will ask you how many VPUS you want to assign to the experiment. Leave atleast 2 VCPUS for backgorund 
	#tasks. Assign the rest to the experiment. You can find number of VCPUS on your system using the command
	#lscpu.
	#The build output is located in build/ directory which would contain the output TimeKeeper kernel module
	#Compiled helper scripts will be located in the scripts directory

3. Install TimeKeeper:
	make install


From here, TimeKeeper should be installed, loaded into the Linux Kernel, and good to go!
```