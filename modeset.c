/*
 * modeset - DRM Modesetting Example
 *
 * Written 2012 by David Herrmann <dh.herrmann@googlemail.com>
 * Dedicated to the Public Domain.
 *
 * DRMֱ����Ⱦ���� Modesettingģʽ���� Example��������
 *
 * ���������� DRM modesetting��API�û�Ӧ�ó����̽ӿ�
 * ��ʹ�ø���API֮ǰ  ��Ҫ����xf86drm.h��xf86drmMode.h 
 * ���߾���libdrm���ṩ  ��Linux���а����������ͷ�ļ�
 * �������������� ���к�����ȫ�ֱ�����"modeset_"Ϊǰ׺
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
 * ����ͼΪ�� ˵��ͨ��libdrm���Կ�����ģʽ���õĹ���
 *
 * 1. ͨ��DVI���½���ʾ������DVI��ͷ���Կ�DVI���ͷ���� �� ��������ӳ���Ϊconnector��Դ
 * 2. �ڻ���DVI��connector�� DRM�ں��豸��������������������DVI�źŵı�����encoder��Դ
 *    �������û�з��� �������connector��������Դ�� �ҵ�����available���õ�encoders��Դ
 *    ���Բ�ǡ���Ľ�encoder��connector֮��Ĺ�ϵ����������е������
 * 3. ������encoder��Ϊͼ��ɨ���ֳ�crtc�����  �ں��������ܻ��encoder������ʵ�crtc��Դ
 *    �������û�з��� �������encoder��������Դ�ϵ�possible_crtc�����ҵ����õ�crtcs��Դ
 *    ���Բ�ǡ���Ľ�crtc��encoder֮��Ĺ�ϵ����������еĴ����
 * 4. crtcɨ���ֳ�(crtc��ʾ������)Ҫ�������ڻ�����ʾͼ��������ڴ��� framebuffer������Դ
 *    ���Բ�ǡ���Ľ�fb��crtc֮��Ĺ�ϵ����������е�Ӧ�ò�
 * 5. ��ͨ��libdrm��fb => crtc => encoder => connector�Ĺ�ϵ��֮��
 *    �༴������һ�����ʵ�pipeline���� ��ͼ������ɿ�ʼ ������fb��������Ʋ����������ʾ
 * 6. Ϊ�˱���ͼ��˺�� ���Խ������fb(����)  ͨ��pageFlip������ˢ�»�ͼ
 * 7. ���⻹��רΪvideo��Ƶˢ���õ�plane, ��planeҲҪ�󶨵�crtc���ܹ��� */


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
#include <xf86drm.h>     /* drm ͷ�ļ� */
#include <xf86drmMode.h> /* ģʽͷ�ļ� */


/* �û��ռ��libdrm�� �ṩһ��drmModeRes�ṹ��  ���������б������Դ
 * ͨ��drmModeGetResources(fd)��ȡ/drmModeFreeResources(res)�ͷ���Դ
 *
 * ͼ���Կ��ϵ�����������������"connector"
 * ͨ�����Խ�һ����ʾ����������������� ����������ʾ������
 * ͨ�������������б� ��������ÿ��������ʾ������ʾ����ͼƬ
 *
 * ������Ҫ����������Ƿ���ʹ����(��ʾ�������Ѳ��벢��)
 * �����Ҫ�ҵ�һ�����������Ƹ�������������CRTC��ʾ������
 * ���ͨ������һ��֡�������
 * ����׼������ ����ͨ��mmap()ӳ��֡���� �������Ͻ��л���
 * ��֪DRM Ҫ�ڸ���CRTC������ѡ�������������ʾ֡�������� 
 * 
 * ����Ҫ��֡�����ϻ����ƶ�ͼƬʱ, ����Ҫ�洢������������
 * ��������������ݽṹ����ÿһ��ƥ����fb+crtc+conn����
 * �����Ѿ��ɹ���ʼ���ļ���  �豸ʵ���������ȫ���豸���� */
struct modeset_dev {
        struct modeset_dev *next;       /* ָ����������һ��Ԫ��  */
        uint32_t            width;      /* ֡�������Ŀ��        */
        uint32_t            height;     /* ֡�������ĸ߶�        */
        uint32_t            stride;     /* ֡�������Ŀ��        */
        uint32_t            size;       /* �ڴ�ӳ�仺���С        */
        uint32_t            handle;     /* ���֡��������DRM��� */
        uint8_t            *map;        /* �ڴ�ӳ�仺���ַ        */
        drmModeModeInfo     mode;       /* ����ʹ�õ���ʾģʽ      */
        uint32_t            fb;         /* ֡������              */
        uint32_t            connid;     /* �������õ���������ʶ��  */
        uint32_t            crtcid;     /* ��������������crtc��ʶ  */
        drmModeCrtc        *saved_crtc; /* �ı�crtc֮ǰ��������Ϣ  */
};


/* ģʽ�����豸ȫ������ */
static struct modeset_dev *modeset_list = NULL;

static int  modeset_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev);
static int  modeset_create_fb(int fd, struct modeset_dev *dev);
static int  modeset_setup_dev(int fd, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev);
static int  modeset_open     (int *out, const char *node);
static int  modeset_prepare  (int fd);
static void modeset_draw     (void);
static void modeset_cleanup  (int fd);


/* ��ͼ���Կ�DRM�豸
 *
 * ��Linux�ں�̽�⵽������е�ͼ���Կ�ʱ �ں˻������Ӧ�Կ��豸����
 * �������ں���/drivers/gpu/drm/<foo>��  �Ҵ��������ַ��豸�����Կ�
 * Udev����(�����κ�ʹ��֧���Ȳ�λ��Ƶ�Ӧ��)���������������豸�ڵ�
 * -> /dev/dri/card0
 * -> /dev/dri/controlID64
 * 
 * �������ʹ�õ�һ���豸�ڵ�  ͨ���ɽ��ýڵ�·��Ӳ������Ӧ�ó�����
 * �����Ƽ�����libudev�������ʵ���Ȳ�μ������֧�� ���ﲻ��ϸ˵��
 * ��ϵͳ�����ö��ͼ���Կ�  ������/dev/dri/card1, /dev/dri/card2��
 *
 * ������ýڵ�·��/dev/dri/card0  ���û���������������ָ�����·��
 * ���豸�� ��Ҫ�����豸�Ƿ�֧�� "DRM_CAP_DUMB_BUFFER" ��������
 * ������֧�ָ����� �������򵥵��ڴ�ӳ�仺�� �������κ������ض�����
 * ��ΪҪ��֤����ͨ���� �ܿ�����radeon, nvidia, intel�ȳ����ض�����
 * ����������DUMB_BUFFERs���� ���������������е�ͨ���� */
static int modeset_open(int *out, const char *node)
{
        int fd, ret;
        uint64_t has_dumb;

        /* ���տɶ���д��ʽ���豸 */
        fd = open(node, O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
                ret = -errno;
                fprintf(stderr, "cannot open '%s': %m\n", node);
                return ret;
        }

        /* ȷ�ϸ�DRM�豸�Ƿ�߱�dumb�������� */
        if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) 
        {
                /* �豸��֧��dumb���� */
                fprintf(stderr, "drm device '%s' does not support dumb buffers\n", node);
                close(fd);
                return -EOPNOTSUPP;
        }

        *out = fd;
        return 0;
}


/* �ø���������Ҫ�����з��ֵ�connector��������һЩʵ��׼������
 * 
 * ���������е������� �����������ĸ�����������ʼ��ÿ��������
 * �ɹ��� ������Ϊһ���豸������뵽ȫ��ģʽ�����豸����֮��
 *
 * ��������Դ�ṹ����Ϣ�� ��������������ID���б�
 * ʹ��drmModeGetConnector()  ����ȡÿһ��������������ϸ����Ϣ
 * ��ȡ����������Դ���������� ��ͨ��drmModeFreeConnector()�ͷ� */
static int modeset_prepare(int fd)
{
        int                 ret;
        unsigned int        i;
        drmModeRes         *reso;
        drmModeConnector   *conn;
        struct modeset_dev *dev;


        /* ��ȡDRM�豸��������Դ
         * ��Դ����fbs + crtcs + encoders + connectors
         * Ȼ����ǰ��Щ��Դ֮�䲢������ ��Ҫ��ƥ�䴦��
         * ͨ����������ʽ����ƥ����ص���Դ */
        reso = drmModeGetResources(fd);
        if (!reso)
        {
                fprintf(stderr, "cannot retrieve DRM resources (%d): %m\n", errno);
                return -errno;
        }

        /* ����DRM�豸�е�����������connector��Դ */
        for (i = 0; i < reso->count_connectors; ++i)
        {
                /* ����DRM�豸��� ����������ID��ʶ��
                 * ��ȡ��ʶ����ָ����������Դ��ϸ��Ϣ */
                fprintf(stderr, "try to get DRM connector[%u]_id %u\n", i, reso->connectors[i]);
                conn = drmModeGetConnector(fd, reso->connectors[i]);
                if (!conn)
                {
                        /* �޷���ȡĳ��ʶ�Ŷ�Ӧ�������� */
                        fprintf(stderr, "cannot retrieve DRM connector %u:%u (%d): %m\n",
                                i, reso->connectors[i], errno);

                        /* ѭ������������һ�������� */
                        continue;
                }

                /* ���䲢��ʼ��һ��ģʽ�����豸�ṹ���� */
                dev = malloc(sizeof(struct modeset_dev));
                memset(dev,0,sizeof(struct modeset_dev));

                /* �������������ID��
                 * ����VMware��SVGAͼ�ο� ����8��������
                 * ÿ����������ID������Ϊ 28,32,36,40,44,48,52,56 */
                dev->connid = conn->connector_id;

                /* Ϊÿһ�����õ������������׼������
                 * ��׼���ɹ�  ��ø�����������ֵΪ�� */
                ret = modeset_setup_dev(fd, reso, conn, dev);
                if (ret)
                {
                        if (ret != -ENOENT)
                        {
                                errno = -ret;
                                fprintf(stderr, "cannot setup device for connector %u:%u (%d): %m\n",
                                        i, reso->connectors[i], errno);
                        }

                        /* ׼��ʧ�� �ͷŷ�����豸��Դ�Լ���������Դ
                         * ������������һ�������� */
                        free(dev);
                        drmModeFreeConnector(conn);
                        continue;
                }

                /* ���������Ѿ����� �ͷ�֮ */
                drmModeFreeConnector(conn);

                /* ���´������豸������ȫ������Ķ��� */
                dev->next = modeset_list;
                modeset_list = dev;
        }

        /* ����������������� �ͷ�DRM�豸������Դ */
        drmModeFreeResources(reso);

        return 0;
}


/* Ϊ��һ������һ��������������  ����Ҫ������¼���Ҫ��
 * 1) ����������ǰδ�� �༴û��monitor���������� �����
 *
 * 2) ��Ҫ�ҵ�һ�����ʵķֱ��ʺ�ˢ���� 
 *    ������Ϣ����ÿ��crtc�������drmModeModeInfo�ṹ��
 *    ������ʹ�õ�һ��ģʽ ��ģʽͨ���Ƿֱ�����ߵ�ģʽ
 *    ������ʵ�ʵ�Ӧ�ù����� ͨ��Ӧ�ý��к����ģʽѡ��
 *
 * 3) �����һ�����ʵ�CRTC���������������connector��Դ
 *    һ��CRTC��ʾ������ ��ÿ��ͼ����ʾ����һ���ڲ���Դ
 *    ͨ��CRTC���Կ��ƶ��ٸ�������connectors �ɷֱ����
 *    �༴ͼ���Կ����е����������� ���ܱ�CRTC������Ҫ��
 *    �༴˵���������е���ʾ������monitor���Ա���������
 *    ͨ���п����ɵ�һ��CRTC �����ƶ��connectors������
 *    ���Խ�������������һ������ ͨ�������ӵ���ʾ������
 *    CRTCs��������������ָ�ʽ���ݿ����������ߵĿ�����
 *    ��������Ŀ��CRTCs��������  ����ͬʱ�������й���
 *    
 * 4) ��ҪΪ������������һ��֡����
 *    ֡������һ���ڴ滺�� ������д��XRGB32��ʽ������Ϣ
 *    ��˿�����֡����frame buffer ����Ⱦ������Ҫ��ͼ��
 *    ���ͨ��CRTC �����ݴ�֡����ɨ���������ʾ�������� */
static int modeset_setup_dev
(
        int                 fd,
        drmModeRes         *reso,
        drmModeConnector   *conn,
        struct modeset_dev *dev
)
{
        int i, ret;


        /* ����Ƿ���monitor��ʾ������������������ */
        if (conn->connection != DRM_MODE_CONNECTED) 
        {
                /* ��������û�в��κμ����� ������δ֪ ����Ը�������
                 * ���ҷ���ָ���Ĵ���� */
                fprintf(stderr, "ignoring unused connector %u\n", conn->connector_id);
                return -ENOENT;
        }

        /* ��������������Ҫ��һ����Ч��ģʽ */
        if (conn->count_modes == 0) 
        {
                /* ��������û����Ч��ģʽ �򷵻�ָ������� */
                fprintf(stderr, "no valid mode for connector %u\n", conn->connector_id);
                return -EFAULT;
        }

        /* ��ӡ��������֧�ֵ�������ʾģʽ */
        for(i = 0; i < conn->count_modes; i++)
        {
                fprintf(stderr, "connector %u modes[%u] name %s\n",
                        conn->connector_id, i, conn->modes[i].name);
        }

        /* ������������һ�����õ�ģʽ��Ϣ��¼���豸����
         * ͨ����һ��ģʽ����ѡ��  �ֱ�����ߵ���ʾģʽ */
        memcpy(&dev->mode, &conn->modes[0], sizeof(dev->mode));

        /* ��¼�ÿ���ģʽ�ķֱ��� */
        dev->width  = conn->modes[0].hdisplay;
        dev->height = conn->modes[0].vdisplay;
        fprintf(stderr, "use mode[0] for connector %u\n", conn->connector_id);

        /* Ϊ����������һ�����ʵ�crtc(�ڲ�����encoder��ƥ�价��) */
        ret = modeset_find_crtc(fd, reso, conn, dev);
        if (ret)
        {
                fprintf(stderr, "no valid crtc for connector %u\n", conn->connector_id);
                return ret;
        }

        /* Ϊ�ҵ���crtc����һ��֡���� */
        ret = modeset_create_fb(fd, dev);
        if (ret)
        {
                fprintf(stderr, "cannot create framebuffer for connector %u\n", conn->connector_id);
                return ret;
        }

        return 0;
}


/* �ø�����������Ϊ���������������Һ��ʵ�CRTC
 * ������������һ����Դ��ظ��� ������encoder
 *
 * Encoder�������ɰ���CRTC�����ݴ�֡����ת��Ϊ�ܹ�����ѡ������ʹ�õ�ǡ����ʽ
 * ʹ�������������Щ���͵�ת�� ����Ҫ֪��ÿ�����������õı������б������޵� 
 * ����ÿһ������������������޵�CRTCs���й���
 * �����Ҫ���Ա������õ�ÿһ�������� ���ҵ�һ�����Ժ͸ñ��������Ϲ�����CRTC
 * ����ҵ���һ����ƥ�乤��������� ���ɽ���ƥ���д���豸�ṹ�����Ӧ��Ϣ��
 * ����������ǰ ����ָ���������ϳ��Ե�ǰ��Ծ��Encoder+Crtc  ������ȫģʽ����
 *
 * ������ʹ��һ��CRTC֮ǰ ���Ҫȷ��û��������ǰ���úõ��豸����ռ�����CRTC
 * ���ֻҪ����ģʽ�б�����CRTC��ǰδռ�� �����������һ�¸�CRTC/Enc��� */
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
        drmModeEncoder     *enc;  /* ������   */
        struct modeset_dev *iter; /* �豸�ڵ� */


        /* �ȳ��Ի�ȡ���������󶨵Ļ�Ծ��Encoder
         * �༴ �ҵ�һ��Encoder + Ctrc���ߵ���� */
        if (conn->encoder_id)
                enc = drmModeGetEncoder(fd, conn->encoder_id);
        else
                enc = NULL;

        if (enc)
        {
                /* �����Ѿ�Ϊ������������һ�����ʵı����� */
                if (enc->crtc_id) 
                {
                        /* ��ǰ�ҵ���һ��Encoder+Ctrc
                         * �༴ͨ������ʽ�Ƴ�conn->enc->crtc */

                        tempid = enc->crtc_id;

                        /* �����豸���� ȷ���Ƿ��Ѿ����豸ռ�ø�CRTC */
                        for (iter = modeset_list; iter; iter = iter->next) 
                        {
                                if (iter->crtcid == tempid) 
                                {
                                        /* �ѱ�ռ�� ���-1 */
                                        tempid = -1;
                                        break;
                                }
                        }

                        if (tempid >= 0) 
                        {
                                /* ��������Դ����ʹ�� �ͷ�֮ */
                                drmModeFreeEncoder(enc);

                                /* ��crtc��δ��ռ�� ��˼�¼��ctrc��ʶ�� */
                                dev->crtcid = tempid;

                                fprintf(stderr, "has been found enc %u and crtc %u for connector %u\n",
                                        enc->encoder_id, enc->crtc_id, conn->connector_id);

                                /* ��Ϊ�������ҵ����ʵ�encoder��crtc ���̷��� */
                                return 0;
                        }
                }

                /* ��������Դ����ʹ�� �ͷ�֮ */
                drmModeFreeEncoder(enc);
        }

        /* ������ִ������  ���������������ǰ��δ�ܰ���һ��������
         * ��Enc+Ctrc�ѱ������������ʹ�� ʵ�ʲ����� ����Ҫ��ȫ����
         * ���������������õı����� ���ҵ�һ��ƥ���CRTC */
        for (i = 0; i < conn->count_encoders; ++i) 
        {
                enc = drmModeGetEncoder(fd, conn->encoders[i]);
                if (!enc)
                {
                        fprintf(stderr, "cannot retrieve encoder %u:%u (%d): %m\n", i, conn->encoders[i], errno);
                        continue;
                }

                /* ��������ȫ�ֵ�CRTCs */
                for (j = 0; j < res->count_crtcs; ++j) 
                {
                        /* ��鵱ǰ��CRTC�Ƿ�������ϸñ��������� */
                        if (!(enc->possible_crtcs & (1 << j)))
                                continue;

                        /* ����Ƿ��������豸��ռ�ø�CRTC */
                        tempid = res->crtcs[j];
                        for (iter = modeset_list; iter; iter = iter->next) 
                        {
                                if (iter->crtcid == tempid) 
                                {
                                        /* �ѱ�ռ�� ���-1 */
                                        tempid = -1;
                                        break;
                                }
                        }

                        /* �ҵ�һ��CRTC ���沢���� */
                        if (tempid >= 0) 
                        {
                                /* ��������Դ����ʹ�� �ͷ�֮ */
                                drmModeFreeEncoder(enc);

                                /* ��crtc��δ��ռ�� ��˼�¼��ctrc��ʶ�� */
                                dev->crtcid = tempid;

                                fprintf(stderr, "iter and found enc %u and crtc %u for connector %u\n",
                                        enc->encoder_id, enc->crtc_id, conn->connector_id);

                                return 0;
                        }
                }

                /* ��������Դ����ʹ�� �ͷ�֮ */
                drmModeFreeEncoder(enc);
        }

        fprintf(stderr, "cannot find suitable CRTC for connector %u\n", conn->connector_id);
        return -ENOENT;
}


/* ���ҵ�һ��crtc->enc->conn��ģʽ��Ϻ� ��Ҫ����һ�����ʵ�֡��������ͼ
 * ͨ�������ַ�ʽ
 * 1) ����һ�ֽ���"dumb"�Ļ��� ���໺�����ͨ��mmapӳ��  ÿ��������֧��
 *    ���������໺����CPU�Ͻ���"��"���ٵ�"��"��Ⱦ
 * 2) ����ͨ��libgbm���������õĻ��� ����Ӳ������
 *    libgbm�Ǹ�Ϊÿ������DRM������������ĳ����
 *    ��Ϊû��ͨ��API  ���ÿ���������ṩ���������еķ�����������Щ����
 *    ��������Щ���� ����Mesa�ⴴ��OpenGL������
 *     
 * ������õ�һ�ֽ������ ��Ϊ��� ����������
 * ����ͨ��OpenGLʹ��Ӳ������  ��ʹ��libgbm��libEGL���ɼ򵥴�����Щ����
 * �˷����ѳ��������������ķ���
 *
 * ������������������һ���µĻ���dumb���͵Ļ���
 * ����ѡ�������ĵ�ǰ�ֱ���ָ��ͬ����С��dumb����
 * �����������������  ��׼��Ϊ�û�������ڴ�ӳ��
 * ���ִ��mmap����ֱ��ͨ��dev->map����֡�����ڴ� */
static int modeset_create_fb(int fd, struct modeset_dev *dev)
{
        int ret;
        struct drm_mode_create_dumb  creq;
        struct drm_mode_destroy_dumb dreq;
        struct drm_mode_map_dumb     mreq;


        /* ����ָ���ֱ��ʴ�С 32λɫ��dumb���� */
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

        /* �ɹ�����dumb����� ��¼pitch size handle����Ϣ */
        dev->stride = creq.pitch;
        dev->size   = creq.size;
        dev->handle = creq.handle;

        /* Ϊdumb���洴��֡������� */
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

        /* ׼�������ڴ�ӳ��Ļ��� */
        memset(&mreq, 0, sizeof(mreq));
        mreq.handle = dev->handle;
        ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
        if (ret)
        {
                fprintf(stderr, "cannot map dumb buffer (%d): %m\n", errno);
                ret = -errno;
                goto err_fb;
        }

        /* ִ��ʵ�ʵ��ڴ�ӳ�� */
        dev->map = mmap(0, dev->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
        if (dev->map == MAP_FAILED) 
        {
                fprintf(stderr, "cannot mmap dumb buffer (%d): %m\n", errno);
                ret = -errno;
                goto err_fb;
        }

        /* ��֡�������� */
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

/* �������к���ʵ����� �༴����Ϊһ��connector�������ҵ���һ�����ʵ�CRTC
 * ����ȷ����ʹ��������ʾģʽ  ��ӵ��һ����ȷ��С֡����д���������Ƶ�����
 * ׼�������Ѿ�����  ��Ϊȫ��ģʽ�����豸�����е�ÿһ����Ͻ��н�һ������ 
 * ͨ��drmModeSetCrtc()��ʵ�� ��Ҫ���CRTC ��ÿ��֡�������ӵ�ѡ���������
 *
 * main����ڿ��Կ�ʼ�����·�ʽ��д��
 * ���ȼ���û���������ָ����DRM�豸�ڵ�·�� ��δָ����ʹ��/dev/dri/card0
 * ͨ��modeset_open()��DRM�豸�ڵ�  ͨ��modeset_prepare()׼������������
 * Ϊÿһ��"crtc->connector"����� ����drmModeSetCrtc()��������֮���ϵ
 *
 * ͨ��modeset_draw()����5����֡�����л���һЩ��ɫ ��������ͷ������Դ
 *
 * drmModeSetCrtcͨ�������õ�crtc���ƴ������������б�  ��Ȼ���������һ��
 * 
 * ��ǰ����� ��ʹ�ö�������� ������������ӵ��ͬһ������֡��  ���Ϊ��¡
 * ͨ�������������� ���������͸�����
 * ����������������ͬһ��ģʽ ͨ���ޱ�֤ ȡ����֮ÿ��crtc������һ��������
 *
 * �ڵ���drmModeSetCrtc()֮ǰ ��Ҫ���浱ǰ��CRTC����
 * ������modeset_cleanup()ʱ�ָ��ı�CRTC״̬ǰ��ģʽ
 * ���������湤�� ����Ļ���Ǹ�����blank״̬ ֱ������һ��Ӧ��ִ��ģʽ���� */
int main(int argc, char **argv)
{
        int                 ret;
        int                 fd;
        const char         *card;
        struct modeset_dev *iter;

        /* ������һ��DRM�豸 ���޲��� ��Ĭ�ϴ�card0 */
        if (argc > 1)
                card = argv[1];
        else
                card = "/dev/dri/card0";

        fprintf(stderr, "using card '%s'\n", card);

        /* ��DRM�豸 */
        ret = modeset_open(&fd, card);
        if (ret)
                goto out_return;

        /* ׼�����е�������connectors����ʾ������CRTCs */
        ret = modeset_prepare(fd);
        if (ret)
                goto out_close;

        /* ����ִ��֧�� ��׼������
         * ���������ɸ� "fb->crtc->encoder->connecotr"
         * ÿһ����������Ӧһ��modeset_devģʽ�����豸
         * ��multi-head��ͷ�Կ� �Ӳ�����������ʾ������ */

        /* Ϊ�ҵ���ÿһ��connector+crtcִ��ʵ�ʵ�ģʽ���� */
        for (iter = modeset_list; iter; iter = iter->next) 
        {
                /* �ȱ���ԭ�е�CRTC���� */
                iter->saved_crtc = drmModeGetCrtc(fd, iter->crtcid);

                /* �������µ�CRTC���� */
                ret = drmModeSetCrtc(fd,           /* drm �豸��� */
                                     iter->crtcid, /* ��crtc��ʶ�� */
                                     iter->fb,     /* ������֡���� */
                                     0,            /* ֡���� xλ�� */
                                     0,            /* ֡���� yλ�� */
                                     &iter->connid,/* ��������ʶ�� */ 
                                     1,            /* ������������ */
                                     &iter->mode); /* ��ģʽ����Ϣ */
                if (ret)
                        fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n", iter->connid, errno);
        }

        /* ����Ļ�ϻ���һЩ��ɫ 
         * ÿ100ms����һ�� ��50�� ����5s������Ļ */
        modeset_draw();

        /* ������Դ */
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


/* ���㽥��ɫ ��������㷨 */
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
 * ���������úõ�֡���������ɫ
 * ÿ100ms ��ɫ������΢�����仯
 * ͨ������������ƥ���ģʽ�б�
 * ��������ÿ�����ص�Ϊ��ǰ��ɫ
 *
 * ��Ϊֱ����֡������� ��ζ�ŵ��ػ���Ļ ��ʾ��ˢ��ʱ���ܻῴ����˸
 * Ϊ����֮ ��Ҫ����֡���� ������drmModeSetCrtc()������֡������л�
 * Ҳ��������drmModePageFlip() ��һ��vsync'ed��ҳ�����pageflip����
 * ���ѳ��������ӵķ��� ������Բο�pageflip�Ĳ������� */
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
                /* ��һ���㷨����rgb��ɫ���� */
                r = next_color(&r_up, r, 20);
                g = next_color(&g_up, g, 10);
                b = next_color(&b_up, b, 5);

                /* �������е���ƥ���ģʽ�б��豸 */
                for (iter = modeset_list; iter; iter = iter->next) 
                {
                        /* ���н�����ɫ���� */
                        for (j = 0; j < iter->height; ++j) 
                        {
                                /* ���н���ÿ�����ص����ɫ���� */
                                for (k = 0; k < iter->width; ++k) 
                                {
                                        off = iter->stride * j + k * 4;

                                        /* ֱ�Ӳ���֡��������Ӧ��ƫ�Ƶ�ַ */
                                        *(uint32_t*)&iter->map[off] = (r << 16) | (g << 8) | b;
                                }
                        }
                }

                /* ÿ����ʱ100ms */
                usleep(100000);
        }
}


/* ����������modeset_prepare()�д�������Դ 
 * �ָ�crtcs����ԭ�б����״̬  ���ͷ��ڴ� */
static void modeset_cleanup(int fd)
{
        struct modeset_dev *iter;
        struct drm_mode_destroy_dumb dreq;

        while (modeset_list) 
        {
                /* ��ȫ���������Ƴ� */
                iter = modeset_list;
                modeset_list = iter->next;

                /* �ָ�ԭ�ȱ����crtc���� */
                drmModeSetCrtc(fd,
                               iter->saved_crtc->crtc_id,
                               iter->saved_crtc->buffer_id,
                               iter->saved_crtc->x,
                               iter->saved_crtc->y,
                               &iter->connid,
                               1,
                               &iter->saved_crtc->mode);

                drmModeFreeCrtc(iter->saved_crtc);

                /* ��ӳ���ڴ� */
                munmap(iter->map, iter->size);

                /* ɾ��֡������� */
                drmModeRmFB(fd, iter->fb);

                /* ����dumb���� */
                memset(&dreq, 0, sizeof(dreq));
                dreq.handle = iter->handle;
                drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);

                /* �ͷ��豸�ڵ�ռ�õ��ڴ� */
                free(iter);
        }
}
