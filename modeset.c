/*
 * modeset - DRM Modesetting Example
 *
 * Written 2012 by David Herrmann <dh.herrmann@googlemail.com>
 * Dedicated to the Public Domain.
 *
 * DRM直接渲染管理 Modesetting模式设置 Example测试用例
 *
 * 本用例描述 DRM modesetting的API用户应用程序编程接口
 * 在使用该套API之前  需要包括xf86drm.h和xf86drmMode.h 
 * 两者均由libdrm库提供  各Linux发行版已捆绑相关头文件
 * 不再有其他依赖 所有函数及全局变量以"modeset_"为前缀
 *
 *        (graphics-card)
 * +---------------------------+                       +-----------+
 * |                           |                       |           |
 * | +--+                      |---+   connector   +---+           |
 * | |fb| => crtc => encoder =>|dvi|  ===========> |dvi|  Monitor  |
 * | +--+                      |---+               +---+           |
 * |                           |                        \_________/ 
 * +---------------------------+                            | |
 *  ||  |||||||||||                                       -------
 *  ++  +++++++++++
 *       PCI-E
 *
 * 以上图为例 说明通过libdrm对显卡进行模式设置的过程
 *
 * 1. 通过DVI线缆将显示监视器DVI插头与显卡DVI输出头连接 并 将这个连接抽象为connector资源
 * 2. 在基于DVI的connector上 DRM内核设备驱动程序会分配用于驱动DVI信号的编码器encoder资源
 *    如果驱动没有分配 则可以在connector连接器资源上 找到所有available可用的encoders资源
 *    可以不恰当的将encoder与connector之间的关系想象成网络中的物理层
 * 3. 编码器encoder是为图像扫描现场crtc服务的  内核驱动可能会给encoder分配合适的crtc资源
 *    如果驱动没有分配 则可以在encoder编码器资源上的possible_crtc上能找到可用的crtcs资源
 *    可以不恰当的将crtc与encoder之间的关系想象成网络中的传输层
 * 4. crtc扫描现场(crtc显示控制器)要配置用于绘制显示图像的物理内存区 framebuffer缓冲资源
 *    可以不恰当的将fb与crtc之间的关系想象成网络中的应用层
 * 5. 当通过libdrm将fb => crtc => encoder => connector的关系绑定之后
 *    亦即构建了一条合适的pipeline管线 绘图工作便可开始 可以在fb上任意绘制并随后立刻显示
 * 6. 为了避免图像撕裂 可以建立多个fb(缓冲)  通过pageFlip操作来刷新画图
 * 7. 另外还有专为video视频刷新用的plane, 而plane也要绑定到crtc才能工作 */


#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>     /* drm 头文件 */
#include <xf86drmMode.h> /* 模式头文件 */


/* 用户空间的libdrm库 提供一个drmModeRes结构体  包含了所有必需的资源
 * 通过drmModeGetResources(fd)获取/drmModeFreeResources(res)释放资源
 *
 * 图形显卡上的物理连接器被称作"connector"
 * 通常可以将一个显示监视器插入该连接器 并控制其显示的内容
 * 通过迭代连接器列表 并尝试在每个可用显示器上显示测试图片
 *
 * 首先需要检查连接器是否在使用中(显示监视器已插入并打开)
 * 其次需要找到一个可用来控制该物理连接器的CRTC显示控制器
 * 最后通过创建一个帧缓冲对象
 * 上述准备就绪 即可通过mmap()映射帧缓冲 并在其上进行绘制
 * 告知DRM 要在给定CRTC控制器选择的连接器上显示帧缓冲内容 
 * 
 * 当需要在帧缓冲上绘制移动图片时, 必须要存储所有上述陪着
 * 因此利用以下数据结构保存每一对匹配后的fb+crtc+conn集合
 * 对于已经成功初始化的集合  设备实例化后放入全局设备链表 */
struct modeset_dev {
        struct modeset_dev *next;       /* 指向单向链表下一个元素  */
        uint32_t            width;      /* 帧缓冲对象的宽度        */
        uint32_t            height;     /* 帧缓冲对象的高度        */
        uint32_t            stride;     /* 帧缓冲对象的跨度        */
        uint32_t            size;       /* 内存映射缓冲大小        */
        uint32_t            handle;     /* 针对帧缓冲对象的DRM句柄 */
        uint8_t            *map;        /* 内存映射缓冲地址        */
        drmModeModeInfo     mode;       /* 期望使用的显示模式      */
        uint32_t            fb;         /* 帧缓冲句柄              */
        uint32_t            connid;     /* 缓冲所用的连接器标识号  */
        uint32_t            crtcid;     /* 连接器被所关联crtc标识  */
        drmModeCrtc        *saved_crtc; /* 改变crtc之前的配置信息  */
};


/* 模式设置设备全局链表 */
static struct modeset_dev *modeset_list = NULL;

static int  modeset_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev);
static int  modeset_create_fb(int fd, struct modeset_dev *dev);
static int  modeset_setup_dev(int fd, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev);
static int  modeset_open     (int *out, const char *node);
static int  modeset_prepare  (int fd);
static void modeset_draw     (void);
static void modeset_cleanup  (int fd);


/* 打开图形显卡DRM设备
 *
 * 当Linux内核探测到计算机中的图形显卡时 内核会加载相应显卡设备驱动
 * 驱动在内核树/drivers/gpu/drm/<foo>下  且创建两个字符设备控制显卡
 * Udev机制(或者任何使用支持热插拔机制的应用)将创建如下两个设备节点
 * -> /dev/dri/card0
 * -> /dev/dri/controlID64
 * 
 * 这里仅需使用第一个设备节点  通常可将该节点路径硬编码至应用程序中
 * 但是推荐采用libudev库完成真实的热插拔及多插座支持 这里不详细说明
 * 若系统中配置多块图形显卡  可能有/dev/dri/card1, /dev/dri/card2等
 *
 * 这里采用节点路径/dev/dri/card0  但用户可以在命令行中指定别的路径
 * 打开设备后 需要检查该设备是否支持 "DRM_CAP_DUMB_BUFFER" 缓冲能力
 * 若驱动支持该能力 将创建简单的内存映射缓存 而无需任何驱动特定代码
 * 因为要保证样例通用性 避开采用radeon, nvidia, intel等厂家特定代码
 * 而仅仅依赖DUMB_BUFFERs缓存 提升测试样例运行的通用性 */
static int modeset_open(int *out, const char *node)
{
        int fd, ret;
        uint64_t has_dumb;

        /* 按照可读可写方式打开设备 */
        fd = open(node, O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
                ret = -errno;
                fprintf(stderr, "cannot open '%s': %m\n", node);
                return ret;
        }

        /* 确认该DRM设备是否具备dumb缓存能力 */
        if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) 
        {
                /* 设备不支持dumb缓存 */
                fprintf(stderr, "drm device '%s' does not support dumb buffers\n", node);
                close(fd);
                return -EOPNOTSUPP;
        }

        *out = fd;
        return 0;
}


/* 该辅助函数主要对所有发现的connector连接器做一些实际准备工作
 * 
 * 随后遍历所有的连接器 并调用其他的辅助函数来初始化每个连接器
 * 成功后 则将其作为一个设备对象加入到全局模式设置设备链表之中
 *
 * 连接器资源结构体信息中 包含所有连接器ID的列表
 * 使用drmModeGetConnector()  来获取每一个连接器更加详细的信息
 * 获取到连接器资源后若不再用 可通过drmModeFreeConnector()释放 */
static int modeset_prepare(int fd)
{
        int                 ret;
        unsigned int        i;
        drmModeRes         *reso;
        drmModeConnector   *conn;
        struct modeset_dev *dev;


        /* 获取DRM设备的所有资源
         * 资源里有fbs + crtcs + encoders + connectors
         * 然而当前这些资源之间并不成套 需要做匹配处理
         * 通常采用逆向方式查找匹配相关的资源 */
        reso = drmModeGetResources(fd);
        if (!reso)
        {
                fprintf(stderr, "cannot retrieve DRM resources (%d): %m\n", errno);
                return -errno;
        }

        /* 遍历DRM设备中的所有连接器connector资源 */
        for (i = 0; i < reso->count_connectors; ++i)
        {
                /* 根据DRM设备句柄 及连接器的ID标识号
                 * 获取标识号所指连接器的资源详细信息 */
                fprintf(stderr, "try to get DRM connector[%u]_id %u\n", i, reso->connectors[i]);
                conn = drmModeGetConnector(fd, reso->connectors[i]);
                if (!conn)
                {
                        /* 无法获取某标识号对应的连接器 */
                        fprintf(stderr, "cannot retrieve DRM connector %u:%u (%d): %m\n",
                                i, reso->connectors[i], errno);

                        /* 循环继续分析下一个连接器 */
                        continue;
                }

                /* 分配并初始化一个模式设置设备结构对象 */
                dev = malloc(sizeof(struct modeset_dev));
                memset(dev,0,sizeof(struct modeset_dev));

                /* 保存该连接器的ID号
                 * 例如VMware的SVGA图形卡 共有8个连接器
                 * 每个连接器的ID号依次为 28,32,36,40,44,48,52,56 */
                dev->connid = conn->connector_id;

                /* 为每一个可用的连接器做相关准备工作
                 * 若准备成功  则该辅助函数返回值为零 */
                ret = modeset_setup_dev(fd, reso, conn, dev);
                if (ret)
                {
                        if (ret != -ENOENT)
                        {
                                errno = -ret;
                                fprintf(stderr, "cannot setup device for connector %u:%u (%d): %m\n",
                                        i, reso->connectors[i], errno);
                        }

                        /* 准备失败 释放分配的设备资源以及连接器资源
                         * 并继续分析下一个连接器 */
                        free(dev);
                        drmModeFreeConnector(conn);
                        continue;
                }

                /* 该连接器已经用完 释放之 */
                drmModeFreeConnector(conn);

                /* 将新创建的设备链接至全局链表的队首 */
                dev->next = modeset_list;
                modeset_list = dev;
        }

        /* 所有连接器分析完毕 释放DRM设备的总资源 */
        drmModeFreeResources(reso);

        return 0;
}


/* 为进一步配置一个独立的连接器  则需要检查以下几点要素
 * 1) 若连接器当前未用 亦即没有monitor监视器插入 则忽略
 *
 * 2) 需要找到一个合适的分辨率和刷新率 
 *    两类信息都在每个crtc所保存的drmModeModeInfo结构中
 *    本用例使用第一个模式 该模式通常是分辨率最高的模式
 *    但是在实际的应用过程中 通常应该进行合理的模式选择
 *
 * 3) 随后找一个合适的CRTC来驱动这个连接器connector资源
 *    一个CRTC显示控制器 是每个图形显示卡的一个内部资源
 *    通常CRTC可以控制多少个连接器connectors 可分别控制
 *    亦即图形显卡含有的连接器数量 可能比CRTC的数量要多
 *    亦即说明不是所有的显示监视器monitor可以被独立控制
 *    通过有可能由单一的CRTC 来控制多个connectors连接器
 *    可以将连接器看作是一条管线 通向所连接的显示监视器
 *    CRTCs则是用来管理何种格式数据可以流过管线的控制器
 *    若管线数目比CRTCs的数量多  则不能同时控制所有管线
 *    
 * 4) 需要为该连接器创建一个帧缓冲
 *    帧缓冲是一个内存缓存 可向其写入XRGB32格式数据信息
 *    因此可以用帧缓冲frame buffer 来渲染我们需要的图形
 *    随后通过CRTC 将数据从帧缓冲扫描输出至显示监视器上 */
static int modeset_setup_dev
(
        int                 fd,
        drmModeRes         *reso,
        drmModeConnector   *conn,
        struct modeset_dev *dev
)
{
        int i, ret;


        /* 检查是否有monitor显示监视器与插入该连接器 */
        if (conn->connection != DRM_MODE_CONNECTED) 
        {
                /* 该连接器没有插任何监视器 或类型未知 则忽略该连接器
                 * 并且返回指定的错误号 */
                fprintf(stderr, "ignoring unused connector %u\n", conn->connector_id);
                return -ENOENT;
        }

        /* 检查该连接器至少要有一个有效的模式 */
        if (conn->count_modes == 0) 
        {
                /* 该连接器没有有效的模式 则返回指定错误号 */
                fprintf(stderr, "no valid mode for connector %u\n", conn->connector_id);
                return -EFAULT;
        }

        /* 打印该连接器支持的所有显示模式 */
        for(i = 0; i < conn->count_modes; i++)
        {
                fprintf(stderr, "connector %u modes[%u] name %s\n",
                        conn->connector_id, i, conn->modes[i].name);
        }

        /* 将该连接器第一个可用的模式信息记录至设备对象
         * 通常第一个模式是优选的  分辨率最高的显示模式 */
        memcpy(&dev->mode, &conn->modes[0], sizeof(dev->mode));

        /* 记录该可用模式的分辨率 */
        dev->width  = conn->modes[0].hdisplay;
        dev->height = conn->modes[0].vdisplay;
        fprintf(stderr, "use mode[0] for connector %u\n", conn->connector_id);

        /* 为该连接器找一个合适的crtc(内部还有encoder的匹配环节) */
        ret = modeset_find_crtc(fd, reso, conn, dev);
        if (ret)
        {
                fprintf(stderr, "no valid crtc for connector %u\n", conn->connector_id);
                return ret;
        }

        /* 为找到的crtc创建一个帧缓冲 */
        ret = modeset_create_fb(fd, dev);
        if (ret)
        {
                fprintf(stderr, "cannot create framebuffer for connector %u\n", conn->connector_id);
                return ret;
        }

        return 0;
}


/* 该辅助函数尝试为给定的连接器查找合适的CRTC
 * 这里需再引入一个资源相关概念 编码器encoder
 *
 * Encoder编码器可帮助CRTC将数据从帧缓冲转换为能够被所选连接器使用的恰当格式
 * 使用中无需理解这些类型的转换 但需要知道每个连接器可用的编码器列表是有限的 
 * 并且每一个编码器仅可配合有限的CRTCs进行工作
 * 因此需要尝试遍历可用的每一个编码器 并找到一个可以和该编码器联合工作的CRTC
 * 如果找到第一个可匹配工作的组合体 即可将该匹配对写入设备结构体的相应信息中
 * 遍历编码器前 需在指定连接器上尝试当前活跃的Encoder+Crtc  避免完全模式设置
 *
 * 另外在使用一个CRTC之前 务必要确保没有其他先前配置好的设备正在占用这个CRTC
 * 因此只要遍历模式列表并检查该CRTC先前未占用 否则继续查找一下个CRTC/Enc组合 */
static int modeset_find_crtc
(
        int                 fd,
        drmModeRes         *res,
        drmModeConnector   *conn,
        struct modeset_dev *dev
)
{
        unsigned int        i, j;
        int32_t             tempid;
        drmModeEncoder     *enc;  /* 编码器   */
        struct modeset_dev *iter; /* 设备节点 */


        /* 先尝试获取连接器所绑定的活跃的Encoder
         * 亦即 找到一对Encoder + Ctrc两者的组合 */
        if (conn->encoder_id)
                enc = drmModeGetEncoder(fd, conn->encoder_id);
        else
                enc = NULL;

        if (enc)
        {
                /* 驱动已经为连接器分配了一个合适的编码器 */
                if (enc->crtc_id) 
                {
                        /* 当前找到了一组Encoder+Ctrc
                         * 亦即通过反向方式推出conn->enc->crtc */

                        tempid = enc->crtc_id;

                        /* 遍历设备链表 确认是否已经有设备占用该CRTC */
                        for (iter = modeset_list; iter; iter = iter->next) 
                        {
                                if (iter->crtcid == tempid) 
                                {
                                        /* 已被占用 标记-1 */
                                        tempid = -1;
                                        break;
                                }
                        }

                        if (tempid >= 0) 
                        {
                                /* 编码器资源不再使用 释放之 */
                                drmModeFreeEncoder(enc);

                                /* 该crtc尚未被占用 因此记录下ctrc标识号 */
                                dev->crtcid = tempid;

                                fprintf(stderr, "has been found enc %u and crtc %u for connector %u\n",
                                        enc->encoder_id, enc->crtc_id, conn->connector_id);

                                /* 已为连接器找到合适的encoder和crtc 立刻返回 */
                                return 0;
                        }
                }

                /* 编码器资源不再使用 释放之 */
                drmModeFreeEncoder(enc);
        }

        /* 若代码执行至此  表明这个连接器当前并未能绑定至一个编码器
         * 若Enc+Ctrc已被另外的连接器使用 实际不可能 但还要安全处理
         * 遍历所有其他可用的编码器 并找到一个匹配的CRTC */
        for (i = 0; i < conn->count_encoders; ++i) 
        {
                enc = drmModeGetEncoder(fd, conn->encoders[i]);
                if (!enc)
                {
                        fprintf(stderr, "cannot retrieve encoder %u:%u (%d): %m\n", i, conn->encoders[i], errno);
                        continue;
                }

                /* 遍历所有全局的CRTCs */
                for (j = 0; j < res->count_crtcs; ++j) 
                {
                        /* 检查当前的CRTC是否可以联合该编码器工作 */
                        if (!(enc->possible_crtcs & (1 << j)))
                                continue;

                        /* 检查是否有其他设备在占用该CRTC */
                        tempid = res->crtcs[j];
                        for (iter = modeset_list; iter; iter = iter->next) 
                        {
                                if (iter->crtcid == tempid) 
                                {
                                        /* 已被占用 标记-1 */
                                        tempid = -1;
                                        break;
                                }
                        }

                        /* 找到一个CRTC 保存并返回 */
                        if (tempid >= 0) 
                        {
                                /* 编码器资源不再使用 释放之 */
                                drmModeFreeEncoder(enc);

                                /* 该crtc尚未被占用 因此记录下ctrc标识号 */
                                dev->crtcid = tempid;

                                fprintf(stderr, "iter and found enc %u and crtc %u for connector %u\n",
                                        enc->encoder_id, enc->crtc_id, conn->connector_id);

                                return 0;
                        }
                }

                /* 编码器资源不再使用 释放之 */
                drmModeFreeEncoder(enc);
        }

        fprintf(stderr, "cannot find suitable CRTC for connector %u\n", conn->connector_id);
        return -ENOENT;
}


/* 当找到一个crtc->enc->conn的模式组合后 需要创建一个合适的帧缓存来绘图
 * 通常有两种方式
 * 1) 创建一种叫做"dumb"的缓存 这类缓存可以通过mmap映射  每种驱动均支持
 *    可以用这类缓存在CPU上进行"非"加速的"软"渲染
 * 2) 可以通过libgbm来创建可用的缓存 用于硬件加速
 *    libgbm是个为每个可用DRM驱动创建缓存的抽象层
 *    因为没有通用API  因此每类驱动都提供了自身特有的方法来创建这些缓存
 *    随后可用这些缓存 借助Mesa库创建OpenGL上下文
 *     
 * 这里采用第一种解决方案 因为其简单 且无需额外库
 * 若需通过OpenGL使用硬件加速  则使用libgbm或libEGL即可简单创建这些缓存
 * 此方面已超出本测试用例的范畴
 *
 * 因此这里从驱动层请求一个新的基于dumb类型的缓存
 * 与所选连接器的当前分辨率指定同样大小的dumb缓存
 * 随后向驱动程序申请  并准备为该缓存进行内存映射
 * 随后执行mmap即可直接通过dev->map访问帧缓存内存 */
static int modeset_create_fb(int fd, struct modeset_dev *dev)
{
        int ret;
        struct drm_mode_create_dumb  creq;
        struct drm_mode_destroy_dumb dreq;
        struct drm_mode_map_dumb     mreq;


        /* 创建指定分辨率大小 32位色的dumb缓存 */
        memset(&creq, 0, sizeof(creq));
        creq.width  = dev->width;
        creq.height = dev->height;
        creq.bpp    = 32;
        ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
        if (ret < 0) 
        {
                fprintf(stderr, "cannot create dumb buffer (%d): %m\n", errno);
                return -errno;
        }

        /* 成功创建dumb缓存后 记录pitch size handle等信息 */
        dev->stride = creq.pitch;
        dev->size   = creq.size;
        dev->handle = creq.handle;

        /* 为dumb缓存创建帧缓冲对象 */
        ret = drmModeAddFB(fd,
                           dev->width,
                           dev->height,
                           24,
                           32,
                           dev->stride,
                           dev->handle,
                           &dev->fb);
        if (ret) 
        {
                fprintf(stderr, "cannot create framebuffer (%d): %m\n", errno);
                ret = -errno;
                goto err_destroy;
        }

        /* 准备用于内存映射的缓冲 */
        memset(&mreq, 0, sizeof(mreq));
        mreq.handle = dev->handle;
        ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
        if (ret)
        {
                fprintf(stderr, "cannot map dumb buffer (%d): %m\n", errno);
                ret = -errno;
                goto err_fb;
        }

        /* 执行实际的内存映射 */
        dev->map = mmap(0, dev->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
        if (dev->map == MAP_FAILED) 
        {
                fprintf(stderr, "cannot mmap dumb buffer (%d): %m\n", errno);
                ret = -errno;
                goto err_fb;
        }

        /* 将帧缓冲清零 */
        memset(dev->map, 0, dev->size);

        return 0;

err_fb:
        drmModeRmFB(fd, dev->fb);

err_destroy:
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = dev->handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return ret;
}

/* 上述所有函数实现完毕 亦即最终为一个connector连接器找到了一个合适的CRTC
 * 且明确期望使用哪种显示模式  并拥有一个正确大小帧缓来写入期望绘制的数据
 * 准备工作已经就绪  需为全局模式设置设备链表中的每一套组合进行进一步设置 
 * 通过drmModeSetCrtc()来实现 需要编程CRTC 将每个帧缓冲连接到选择的连接器
 *
 * main主入口可以开始按如下方式编写了
 * 首先检查用户在命令行指定的DRM设备节点路径 若未指定则使用/dev/dri/card0
 * 通过modeset_open()打开DRM设备节点  通过modeset_prepare()准备所有连接器
 * 为每一个"crtc->connector"的组合 调用drmModeSetCrtc()来绑定两者之间关系
 *
 * 通过modeset_draw()持续5秒在帧缓冲中绘制一些颜色 最后清理并释放相关资源
 *
 * drmModeSetCrtc通过被设置的crtc控制传进的连接器列表  虽然这里仅传递一个
 * 
 * 如前面解释 若使用多个连接器 则所有连接器拥有同一个控制帧缓  输出为克隆
 * 通常不期望这样做 这里避免解释该特性
 * 所有连接器运行在同一个模式 通常无保证 取而代之每个crtc仅用在一个连接器
 *
 * 在调用drmModeSetCrtc()之前 还要保存当前的CRTC配置
 * 便于在modeset_cleanup()时恢复改变CRTC状态前的模式
 * 若不做保存工作 则屏幕家那个保持blank状态 直到另外一个应用执行模式设置 */
int main(int argc, char **argv)
{
        int                 ret;
        int                 fd;
        const char         *card;
        struct modeset_dev *iter;

        /* 检查打开哪一个DRM设备 若无参数 则默认打开card0 */
        if (argc > 1)
                card = argv[1];
        else
                card = "/dev/dri/card0";

        fprintf(stderr, "using card '%s'\n", card);

        /* 打开DRM设备 */
        ret = modeset_open(&fd, card);
        if (ret)
                goto out_return;

        /* 准备所有的连接器connectors和显示控制器CRTCs */
        ret = modeset_prepare(fd);
        if (ret)
                goto out_close;

        /* 代码执行支持 已准备就绪
         * 构建了若干个 "fb->crtc->encoder->connecotr"
         * 每一个数据链对应一个modeset_dev模式设置设备
         * 如multi-head多头显卡 接驳若干类型显示监视器 */

        /* 为找到的每一对connector+crtc执行实际的模式设置 */
        for (iter = modeset_list; iter; iter = iter->next) 
        {
                /* 先备份原有的CRTC配置 */
                iter->saved_crtc = drmModeGetCrtc(fd, iter->crtcid);

                /* 再设置新的CRTC配置 */
                ret = drmModeSetCrtc(fd,           /* drm 设备句柄 */
                                     iter->crtcid, /* 新crtc标识号 */
                                     iter->fb,     /* 关联的帧缓冲 */
                                     0,            /* 帧缓冲 x位置 */
                                     0,            /* 帧缓冲 y位置 */
                                     &iter->connid,/* 连接器标识号 */ 
                                     1,            /* 连接器的数量 */
                                     &iter->mode); /* 新模式的信息 */
                if (ret)
                        fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n", iter->connid, errno);
        }

        /* 在屏幕上绘制一些颜色 
         * 每100ms更新一次 共50次 持续5s绘制屏幕 */
        modeset_draw();

        /* 清理资源 */
        modeset_cleanup(fd);

        ret = 0;

out_close:
        close(fd);

out_return:
        if (ret) 
        {
                errno = -ret;
                fprintf(stderr, "modeset failed with error %d: %m\n", errno);
        } else 
        {
                fprintf(stderr, "exiting\n");
        }

        return ret;
}


/* 计算渐变色 无需理解算法 */
static uint8_t next_color(bool *up, uint8_t cur, unsigned int mod)
{
        uint8_t next;

        next = cur + (*up ? 1 : -1) * (rand() % mod);
        if ((*up && next < cur) || (!*up && next > cur)) 
        {
                *up = !*up;
                next = cur;
        }

        return next;
}


/*
 * 向所有配置好的帧缓冲绘制颜色
 * 每100ms 颜色渐变略微发生变化
 * 通过遍历所有已匹配的模式列表
 * 逐行设置每个像素点为当前颜色
 *
 * 因为直接向帧缓冲绘制 意味着当重绘屏幕 显示器刷新时可能会看到闪烁
 * 为避免之 需要两个帧缓冲 并调用drmModeSetCrtc()在两个帧缓冲间切换
 * 也可以利用drmModePageFlip() 做一个vsync'ed的页面剪切pageflip操作
 * 这已超出本例子的范畴 具体可以参考pageflip的测试用例 */
static void modeset_draw(void)
{
        uint8_t             r, g, b;
        bool                r_up, g_up, b_up;
        unsigned int        i, j, k, off;
        struct modeset_dev *iter;


        srand(time(NULL));
        r = rand() % 0xff;
        g = rand() % 0xff;
        b = rand() % 0xff;
        r_up = g_up = b_up = true;

        for (i = 0; i < 50; ++i) 
        {
                /* 按一定算法生成rgb颜色分量 */
                r = next_color(&r_up, r, 20);
                g = next_color(&g_up, g, 10);
                b = next_color(&b_up, b, 5);

                /* 遍历所有的已匹配的模式列表设备 */
                for (iter = modeset_list; iter; iter = iter->next) 
                {
                        /* 逐行进行颜色设置 */
                        for (j = 0; j < iter->height; ++j) 
                        {
                                /* 逐列进行每个像素点的颜色设置 */
                                for (k = 0; k < iter->width; ++k) 
                                {
                                        off = iter->stride * j + k * 4;

                                        /* 直接操作帧缓冲中相应的偏移地址 */
                                        *(uint32_t*)&iter->map[off] = (r << 16) | (g << 8) | b;
                                }
                        }
                }

                /* 每次延时100ms */
                usleep(100000);
        }
}


/* 清理所有在modeset_prepare()中创建的资源 
 * 恢复crtcs至其原有保存的状态  并释放内存 */
static void modeset_cleanup(int fd)
{
        struct modeset_dev *iter;
        struct drm_mode_destroy_dumb dreq;

        while (modeset_list) 
        {
                /* 从全局链表中移除 */
                iter = modeset_list;
                modeset_list = iter->next;

                /* 恢复原先保存的crtc配置 */
                drmModeSetCrtc(fd,
                               iter->saved_crtc->crtc_id,
                               iter->saved_crtc->buffer_id,
                               iter->saved_crtc->x,
                               iter->saved_crtc->y,
                               &iter->connid,
                               1,
                               &iter->saved_crtc->mode);

                drmModeFreeCrtc(iter->saved_crtc);

                /* 反映射内存 */
                munmap(iter->map, iter->size);

                /* 删除帧缓冲对象 */
                drmModeRmFB(fd, iter->fb);

                /* 销毁dumb缓存 */
                memset(&dreq, 0, sizeof(dreq));
                dreq.handle = iter->handle;
                drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);

                /* 释放设备节点占用的内存 */
                free(iter);
        }
}
