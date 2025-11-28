Ubuntu配置Udev规则固定设备口
一、查看自己的设备
1. 查看当前的串口设备
lsusb
[可选]查看所有的tty设备
ll /dev/tty*
[可选]查看更详细的信息
lsusb -vvv
[可选]查看雷达在系统中挂载的实际挂载情况
ls -l /dev
或
ls -l /dev/tty*
一般雷达以/dev/ttyUSB0的身份挂载
2. 拔掉自己的设备，再次查看当前的串口设备
lsusb
确定消失的行即是所要设置的自己的串口设备
并确定这个设备的idVendor与idProduct（后文将会提到）
在这里插入图片描述
二、设置Udev规则
Udev全称可以认为是UserDevice
Udev规则用于配置外设
1. 进入系统的Udev文件夹
cd /etc/udev/rules.d
2. 建立一个新的规则文件
sudo touch new_device_udev_rules.rules
sudo gedit new_device_udev_rules.rules
修改内容为：
KERNEL=="ttyUSB*", SUBSYSTEMS=="usb", ATTRS{
   
   idVendor}=="067b", ATTRS{
   
   idProduct}=="2303", MODE:="0777", SYMLINK+="device_name"
设备名称、子系统、设备的VID、设备的PID、权限模式和符号链接
① KERNEL与SUBSYSTEMS

KERNEL中的ttyUSB*
usb设备通常默认为上述参数，特定的设备可能有独特的参数要求
通常 SUBSYSTEMS==“usb”，可以不写，如设备另有要求需要特殊设置
② ATTRS{idVendor}与ATTRS{idProduct}

在这里插入图片描述
以上图为例 0e0f为idVendor 0002为idProduct
ATTRS{idVendor}可通过lsvvv
③ MODE

MODE代表设备的权限， 0777为设置最高权限
此处可以设置为 MODE:=“0777”
④ SYMLINK

SYMLINK中device_name对应的是自己命名的设备名称
将以/dev/device_name的名称被你应用

注：某些电子设备已经配置好了自己的udev文件，比如Teensy，可以在电子设备对应的网站上查找
注：某些设备可能需要设置更多参数
三、加载并重启Udev规则
1. 拔掉要固定的设备，并执行如下命令

sudo service udev reload
sudo service udev restart
or
udevadm control --reload-rules && udevadm trigger
2. 重新插上要固定的设备

ls /dev/device_name
3. 查看是否配置成功
/dev/device_name

则说明配置成功
权限
了解权限

如果要了解雷达权限，使用如下命令：

ls -l /dev/tty*

一般雷达以/dev/ttyUSB0的身份挂载
设置权限

将雷达权限设置为crwxrwxrwx，即在任何用户情况下，可读可写可执行

sudo chmod 777 /dev/ttyUSB0
