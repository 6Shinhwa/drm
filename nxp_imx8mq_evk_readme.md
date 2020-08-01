## NXP&reg; i.MX 8M Quad Evaluation Kit (EVK) - nxp_imx8 VxWorks&reg; 7 Board Support Package (BSP)  

## Supported Boards

The **nxp_imx8** board support package (BSP) can be used to boot VxWorks 7 on the **NXP i.MX 8M Quad Evaluation Kit (EVK)**.


## Supported Devices

| Hardware Interface     | VxBus Driver Name | VxWorks Driver Component | VxBus Driver Module Source File |
| ---------------------- | ---------------------- | ----------- | ----------- |
| UART                 | imx-sio | DRV_SIO_FDT_FSL_IMX | vxbFdtFslImxSio.c |
| ARM Generic Timer   | armGenTimer | DRV_ARM_GEN_TIMER | vxbFdtArmGenTimer.c |
| GPT            | imx-gpt | DRV_TIMER_FDT_FSL_IMX_GPT  | vxbFdtFslImxGpt.c |
| INTERRUPT CONTROLLER   | armgic-v3 | DRV_INTCTLR_FDT_ARM_GIC_V3 | vxbFdtArmGicv3.c |
| GIC V3 ITS | armgic-v3-its | DRV_INTCTLR_FDT_ARM_GIC_V3_ITS | vxbFdtArmGicv3Its.c |
| ETHERNET       | enet | DRV_END_FDT_FSL_IMX | vxbFdtFslImxEnd.c |
| GPIO           | imx-gpio | DRV_GPIO_FDT_FSL_IMX  | vxbFdtFslImxGpio.c |
| SDMA   | imx-sdma | DRV_DMA_FDT_FSL_IMX | vxbFdtFslImxSdma.c |
| CLOCK        | imx8mq-ccm | DRV_CLK_FDT_FSL_IMX8  | vxbFdtFslImx8Clk.c |
| PinMux       | imx-iomux | DRV_PINMUX_FDT_FSL_IMX | vxbFdtFslImxPinmux.c |
| PCIe  | imx-pcie | DRV_FDT_FSL_IMX_PCIE | vxbFdtFslImxPcie.c |
| QSPI |  | DRV_FDT_NXPQSPI | vxbFdtNxpQspi.c |
| SD/eMMC | imxSdhc | DRV_IMX_SDHC_CTRL | vxbFslImxSdhcCtrl.c |
| USB Phy | imx8m-usbPhy | INCLUDE_USB_PHY_NXP_IMX8M | vxbFdtUsbPhyNxpImx8m.c |
| USB Type-C | usb-tcpci | INCLUDE_USB_TCPC_INTERFACE | usbTcpci.c |
| USB xHCI | dwc3Simple | INCLUDE_USB_XHCI_HCD | dwc3Simple.c |

For more information about how to configure these VxBus drivers beyond their defaults, refer to the [NXP nxp_imx8 BSP Supplement Guide](https://docs.windriver.com/bundle/nxp_nxp_imx8_bsp_supplement_@vx_release "NXP nxp_imx8 BSP Supplement Guide") on the **Wind River Support Network**.


## Software Prerequisites

Before you begin, you must do the following:  

  * Install Wind River VxWorks 7 on the workstation.  

  * Open a workstation command terminal and configure the environment variables for VxWorks 7 command-line interface (CLI) development. Refer to the [NXP nxp_imx8 BSP Supplement Guide](https://docs.windriver.com/bundle/nxp_nxp_imx8_bsp_supplement_@vx_release "NXP nxp_imx8 BSP Supplement Guide") for details.  
  
  * Install a TFTP server program on the workstation.  

  * Be familiar with the process for building and configuring VxWorks projects. For more information, see [VxWorks 7 Configuration and Build Guide](https://docs.windriver.com/bundle/vxworks_7_configuration_and_build_guide_@vx_release "VxWorks 7 Configuration and Build Guide").  

  * Be familiar with using U-Boot to load an operating system to a target board. For more information, see [The DENX U-Boot and Linux Guide](https://www.denx.de/wiki/DULG/Manual "The DENX U-Boot and Linux Guide").  

  * Locate the Board Support Package L4.14.98_2.0.0_IMX8MQ_GA(REV L4.14.98_2.0.0 on the NXP website MCIMX8M-EVK: Evaluation Kit for the i.MX 8M Applications Processor, Software & Tools.


## Hardware Prerequisites

  * Make sure you have the NXP supplied SD card. You will boot the board from this SD card.  

  * Make sure you have Rev A (700-29615) of the i.MX8MQ EVK board, type MCIMX8M-EVK.


## Target Boot Procedure
This procedure is the most direct strategy for booting the target board successfully to the VxWorks kernel shell prompt. It can be done from either Linux or Windows workstations.

### Step 1: Set up the Target
1.1 Attach the serial cable between the target's UART port (J1701) and the workstation, and attach a physical network cable to the target.  

1.2 Start a serial terminal emulator program on your workstation.

Set the serial communication configuration as follows:

    Baud Rate: 115200
    Data: 8 bit
    Parity: None
    Stop: 1 bit
    Flow Control: None

1.3 Set switch SW801 as follows:

<table>
  <tr>
    <th>Switch Number</th>
    <th>Setting [ 1 2 3 4 ]</th>
  </tr>
  <tr>
    <td align="center">SW801</td>
    <td align="center">1 1 0 0</td>
  </tr>
</table>

1.4 Extract imx-boot-imx8mqevk-sd.bin-flash_evk from the Board Support Package you downloaded from the NXP website.  

1.5 Insert the SD card to the workstation and deploy the file to it.  
  
    ~$ sudo dd if=imx-boot-imx8mqevk-sd.bin-flash_evk of=/dev/sdb bs=1k seek=33 conv=fsync
  
**NOTE 1:** Replace /dev/sdb with your own SD card device name.   

**NOTE 2:** Wind River recommends you deploy the U-Boot image from a Linux workstation.  

1.6 Eject the SD card and insert to the board SDHC card socket, power up and watch U-Boot start.  

        U-Boot 2018.03-imx_v2018.03_4.14.98_2.0.0_ga+g87a19df (Apr 11 2019 - 10:54:20 +0000)
        
	CPU:   Freescale i.MX8MQ rev2.0 1500 MHz (running at 1000 MHz)
        CPU:   Commercial temperature grade (0C to 95C) at 23C
        Reset cause: POR
        Model: Freescale i.MX8MQ EVK
        DRAM:  3 GiB
        TCPC:  Vendor ID [0x1fc9], Product ID [0x5110], Addr [I2C0 0x50]
	MMC:   FSL_SDHC: 0, FSL_SDHC: 1
        No panel detected: default to HDMI
        Display: HDMI (1280x720)
        In:    serial
        Out:   serial
        Err:   serial

         BuildInfo:
         - ATF 1cb68fa
         - U-Boot 2018.03-imx_v2018.03_4.14.98_2.0.0_ga+g87a19df

        switch to partitions #0, OK
        mmc1 is current device
        Net:   eth0: ethernet@30be0000
        Normal Boot
        Hit any key to stop autoboot:  1


### Step 2: Create the VxWorks Source Build Project
2.1 From the workstation command terminal, enter the following:

    ~/WindRiver/workspace$ vxprj vsb create -bsp nxp_imx8 myVSB -S
    ~/WindRiver/workspace$ cd myVSB
    ~/WindRiver/workspace/myVSB$ vxprj build
    ~/WindRiver/workspace/myVSB$ cd ..


### Step 3: Create the VxWorks Image Project
3.1 From the workstation command terminal, enter the following:

    ~/WindRiver/workspace$ vxprj create -vsb myVSB nxp_imx8 myVIP
    ~/WindRiver/workspace$ cd myVIP  


### Step 4: Configure VxWorks Image Project with the Target Board MAC Address
4.1 Open the following file in an editor:

    ~/WindRiver/workspace/myVIP/nxp_imx8_w_x_y_z/imx8mq.dtsi

**NOTE 1:** w,x,y,z represent the **nxp_imx8** BSP layer version number.  

4.2 Locate the **enet0** ethernet device and modify the **local-mac-address** property to reflect the actual ethernet address (MAC address) of your target board:

    enet0: ethernet@30be0000
        {
        ....
        local-mac-address = [ AA BB CC DD EE FF ]; 


### Step 5: Build the VxWorks Image Project
5.1 Build the U-Boot compatible VIP VxWorks image:

    ~/WindRiver/workspace/myVIP$ vxprj build uVxWorks

### Step 6: Prepare to Boot the VxWorks Image File Using U-Boot
6.1 Make sure the TFTP server is running on the workstation and is set to serve files from the following directory:

    ~/WindRiver/workspace/myVIP/default  

6.2 Power up the target board into the U-Boot shell.  

6.3 Set the target board network parameters from the U-Boot shell:

    => setenv ipaddr 192.168.0.3
    => setenv netmask 255.255.255.0
    => setenv serverip 192.168.0.2
    => setenv bootargs 'enet(0,0)host:vxWorks h=192.168.0.2 e=192.168.0.3:ffffff00 u=target pw=vxTarget f=0x00'
    => saveenv

**NOTE 1:** These boot parameters assume a workstation IP address of **192.168.0.2** and target board IP address of **192.168.0.3**. Adjust these IP addresses based on your network configuration.  
  
**NOTE 2:** If you set bootargs incorrectly, this can cause the target network connection to fail. If this occurs, you must clear bootargs in U-Boot so that VxWorks will default to the bootline specified in the device tree file.

    => setenv bootargs
    => saveenv

### Step 7: Boot and Run VxWorks
7.1 Load the VxWorks image and DTB files from the workstation using the U-Boot shell:

    => tftpboot 0x43000000 uVxWorks
    => tftpboot 0x41000000 imx8mq-evk.dtb

7.2 Run the VxWorks image using the U-Boot shell:

    => bootm 0x43000000 - 0x41000000

7.3 Once VxWorks has booted, the banner becomes visible on the target console.

    Target Name: vxTarget


                              VxWorks 7 SMP
            
                Copyright 1984-2018 Wind River Systems,  Inc.
            
                     Core Kernel version: 1.2.7.0
                              Build date: Aug 10 2018 13:53:49
                                   Board: 
                               CPU Count: 4
                          OS Memory Size: 
                        ED&R Policy Mode: Permanantly Deployed
                    
                    
                    
    Adding 9870 symbols for standalone.

    ->


**NOTE 1:** For a description of alternative BSP boot workflows, refer to the [NXP nxp_imx8 BSP Supplement Guide](https://docs.windriver.com/bundle/nxp_nxp_imx8_bsp_supplement_@vx_release "NXP nxp_imx8 BSP Supplement Guide") on the **Wind River Support Network**.

## Additional Documents
| Document Name                                                |
| :----------------------------------------------------------- |
| [VxWorks 7 Boot Loader User's Guide](https://docs.windriver.com/bundle/vxworks_7_boot_loader_users_guide_@vx_release "VxWorks 7 Boot Loader User's Guide") |
| [NXP nxp_imx8 BSP Supplement Guide](https://docs.windriver.com/bundle/nxp_nxp_imx8_bsp_supplement_@vx_release "NXP nxp_imx8 BSP Supplement Guide") |
| [VxWorks 7 Configuration and Build Guide](https://docs.windriver.com/bundle/vxworks_7_configuration_and_build_guide_@vx_release "VxWorks 7 Configuration and Build Guide") |
| [The DENX U-Boot and Linux Guide](https://www.denx.de/wiki/DULG/Manual "The DENX U-Boot and Linux Guide") |
| MCIMX8M-EVK: Evaluation Kit for the i.MX 8M Applications Processor, Software & Tools |
| i.MX 8M Dual/8M QuadLite/8M Quad Applications Processors Reference Manual Rev. 0, 01/2018 |
| i.MX 8M EVK Board Hardware User's Guide Rev.1, 05/2018 |
| i.MX8M Customer Board Schematics DevBoard Rev B3 |

