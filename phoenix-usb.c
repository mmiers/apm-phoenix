/*
 * Phoenix RC USB reader
 *
 * Based on:
 *
 * Hidraw Userspace Example
 *
 * Copyright (c) 2010 Alan Ott <alan@signal11.us>
 * Copyright (c) 2010 Signal 11 Software
 *
 * The code may be used by anyone for any purpose,
 * and can serve as a starting point for developing
 * applications using hidraw.
 *
 * Some information from OpenPilot (report to read for USB info)
 */

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/*
 * Ugly hack to work around failing compilation on systems that don't
 * yet populate new version of hidraw.h to userspace.
 */
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif

/* Unix */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

const char *bus_str(int bus);
void pwm_init(char *addr);
void pwm_send(char *buf, int size);

int pwm_fd;
struct sockaddr_in pwm_addr;

int
main(int argc, char **argv)
{
    int fd;
    int i, res, desc_size = 0;
    char buf[256], *dev;
    struct hidraw_report_descriptor rpt_desc;
    struct hidraw_devinfo info;

    /* Usage check */
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hidraw-dev> <SITL IP>\n", argv[0]);
        return 1;
    }

    /* Device is argv[1] */
    dev = argv[1];
    /* IP addr is argv[2] */
    pwm_init(argv[2]);

    /* Open the Device with non-blocking reads. In real life,
       don't use a hard coded path; use libudev instead. */
    fd = open(dev, O_RDWR|O_NONBLOCK);

    if (fd < 0)
    {
        perror("Unable to open device");
        return 1;
    }

    memset(&rpt_desc, 0x0, sizeof(rpt_desc));
    memset(&info, 0x0, sizeof(info));
    memset(buf, 0x0, sizeof(buf));

    /* Get Report Descriptor Size */
    res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
    if (res < 0)
    {
        perror("HIDIOCGRDESCSIZE");
    }
    else
    {
        printf("Report Descriptor Size: %d\n", desc_size);
    }

    /* Get Report Descriptor */
    rpt_desc.size = desc_size;
    res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
    if (res < 0)
    {
        perror("HIDIOCGRDESC");
    }
    else
    {
        printf("Report Descriptor:\n");
        for (i = 0; i < rpt_desc.size; i++)
        {
            printf("%hhx ", rpt_desc.value[i]);
        }
        puts("\n");
    }

    /* Get Raw Name */
    res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
    if (res < 0)
    {
        perror("HIDIOCGRAWNAME");
    }
    else
    {
        printf("Raw Name: %s\n", buf);
    }

    /* Get Physical Location */
    res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
    if (res < 0)
    {
        perror("HIDIOCGRAWPHYS");
    }
    else
    {
        printf("Raw Phys: %s\n", buf);
    }

    /* Get Raw Info */
    res = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (res < 0)
    {
        perror("HIDIOCGRAWINFO");
    }
    else
    {
        printf("Raw Info:\n");
        printf("\tbustype: %d (%s)\n",
            info.bustype, bus_str(info.bustype));
        printf("\tvendor: 0x%04hx\n", info.vendor);
        printf("\tproduct: 0x%04hx\n", info.product);
    }

    /* Set Feature */
    buf[0] = 0x9; /* Report Number */
    buf[1] = 0xff;
    buf[2] = 0xff;
    buf[3] = 0xff;
    res = ioctl(fd, HIDIOCSFEATURE(4), buf);
    if (res < 0)
    {
        perror("HIDIOCSFEATURE");
    }
    else
    {
        printf("ioctl HIDIOCGFEATURE returned: %d\n", res);
    }

    /* Get Feature */
    buf[0] = 0x9; /* Report Number */
    res = ioctl(fd, HIDIOCGFEATURE(256), buf);
    if (res < 0)
    {
        perror("HIDIOCGFEATURE");
    }
    else
    {
        printf("ioctl HIDIOCGFEATURE returned: %d\n", res);
        printf("Report data (not containing the report number):\n\t");
        for (i = 0; i < res; i++)
        {
            printf("%hhx ", buf[i]);
        }
        puts("\n");
    }

    /* Send a Report to the Device */
    buf[0] = 0x1; /* Report Number */
    buf[1] = 0x77;
    res = write(fd, buf, 2);
    if (res < 0)
    {
        printf("Error: %d\n", errno);
        perror("write");
    }
    else
    {
        printf("write() wrote %d bytes\n", res);
    }

    /* Get a report from the device */
    res = read(fd, buf, 16);
    if (res < 0)
    {
        perror("read");
    }
    else
    {
        printf("read() read %d bytes:\n\t", res);
        for (i = 0; i < res; i++)
        {
            printf("%hhx ", buf[i]);
        }
        puts("\n");
    }

    /* OP loop */
    while (1)
    {
        /* Send a Report to the Device */
        buf[0] = 0x0; /* Report Number */
        buf[1] = 0x0;
        res = write(fd, buf, 2);
        if (res < 0)
        {
            printf("Error: %d\n", errno);
            perror("write");
        }

        /* Get a report from the device */
        res = read(fd, buf, 16);
        if (res < 0)
        {
            perror("read");
        }
        else
        {
            pwm_send(buf, res);
        }

        usleep(20 * 1000);
    }

    close(fd);
    return 0;
}

const char *
bus_str(int bus)
{
    switch (bus)
    {
    case BUS_USB:
        return "USB";
        break;
    case BUS_HIL:
        return "HIL";
        break;
    case BUS_BLUETOOTH:
        return "Bluetooth";
        break;
    case BUS_VIRTUAL:
        return "Virtual";
        break;
    default:
        return "Other";
        break;
    }
}

void
pwm_init(char *addr)
{
    pwm_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (pwm_fd < 0)
    {
        perror("PWM socket: ");
        exit(1);
    }

    pwm_addr.sin_family = AF_INET;
    pwm_addr.sin_addr.s_addr = inet_addr(addr);
    pwm_addr.sin_port = htons(5501);
}

void
pwm_send(char *buf, int size)
{
    struct pwm_data
    {
        uint16_t pwm[8];
    } pkt;
    int i;

    pkt.pwm[0] = ((uint16_t)(buf[0]) * 2900) / 255;
    for (i = 1; i < 7; i++)
    {
        pkt.pwm[i] = ((uint16_t)(buf[i+1]) * 2900) / 255;
    }
    pkt.pwm[7] = 1500;

#if 0
    sendto(pwm_fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sin, sizeof(sin));
#else
    for (i = 0; i < 8; i++)
    {
        printf("%hd ", pkt.pwm[i]);
    }
    printf("\r");
#endif
}

