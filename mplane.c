/* csdn: 专题讲解
 * https://blog.csdn.net/ldl617/category_11380464.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <errno.h>

struct plane_start {
	void * start;
};

struct buffer {
	struct plane_start* plane_start;
	struct v4l2_plane* planes_buffer;
};

int main(int argc, char **argv)
{
	int fd;
	fd_set fds;
	FILE *file_fd;
	struct timeval tv;
	int ret = -1, i, j, r;
	int num_planes;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	struct v4l2_plane* planes_buffer;
	struct plane_start* plane_start;
	struct buffer *buffers;
	enum v4l2_buf_type type;

	if (argc != 4) {
		printf("Usage: v4l2_test <device> <frame_num> <save_file>\n");
		printf("example: v4l2_test /dev/video0 10 test.yuv\n");
		return ret;
	}

	fd = open(argv[1], O_RDWR);

	if (fd < 0) {
		printf("open device: %s fail\n", argv[1]);
		goto err;
	}

	file_fd = fopen(argv[3], "wb+");
	if (!file_fd) {
		printf("open save_file: %s fail\n", argv[3]);
		goto err1;
	}

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		printf("Get video capability error!\n");
		goto err1;
	}

	if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		printf("Video device not support capture!\n");
		goto err1;
	}

	printf("Support capture!\n");

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = 2400;
	fmt.fmt.pix_mp.height = 1920;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SRGGB12;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		printf("Set format fail\n");
		goto err1;
	}

	printf("width = %d\n", fmt.fmt.pix_mp.width);
	printf("height = %d\n", fmt.fmt.pix_mp.height);
	printf("nmplane = %d\n", fmt.fmt.pix_mp.num_planes);

	//memset(&fmt, 0, sizeof(struct v4l2_format));
	//fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	//if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
	//	printf("Set format fail\n");
	//	goto err;
	//}

	//printf("nmplane = %d\n", fmt.fmt.pix_mp.num_planes);

	req.count = 5;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		printf("Reqbufs fail\n");
		goto err1;
	}

	printf("buffer number: %d\n", req.count);

	num_planes = fmt.fmt.pix_mp.num_planes;

	buffers = malloc(req.count * sizeof(*buffers));

	for(i = 0; i < req.count; i++) {
		memset(&buf, 0, sizeof(buf));
		planes_buffer = calloc(num_planes, sizeof(*planes_buffer));
		plane_start = calloc(num_planes, sizeof(*plane_start));
		memset(planes_buffer, 0, sizeof(*planes_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.m.planes = planes_buffer;
		buf.length = num_planes;
		buf.index = i;
		if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)) {
			printf("Querybuf fail\n");
			req.count = i;
			goto err2;
		}

		(buffers + i)->planes_buffer = planes_buffer;
		(buffers + i)->plane_start = plane_start;
		for(j = 0; j < num_planes; j++) {
			printf("plane[%d]: length = %d\n", j, (planes_buffer + j)->length);
			printf("plane[%d]: offset = %d\n", j, (planes_buffer + j)->m.mem_offset);
			(plane_start + j)->start = mmap (NULL /* start anywhere */,
									(planes_buffer + j)->length,
									PROT_READ | PROT_WRITE /* required */,
									MAP_SHARED /* recommended */,
									fd,
									(planes_buffer + j)->m.mem_offset);
			if (MAP_FAILED == (plane_start +j)->start) {
				printf ("mmap failed\n");
				req.count = i;
				goto unmmap;
			}
		}
	}

	for (i = 0; i < req.count; ++i) {
		memset(&buf, 0, sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.length   	= num_planes;
		buf.index       = i;
		buf.m.planes 	= (buffers + i)->planes_buffer;

		if (ioctl (fd, VIDIOC_QBUF, &buf) < 0)
			printf ("VIDIOC_QBUF failed\n");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
		printf ("VIDIOC_STREAMON failed\n");

	int num = 0;
	struct v4l2_plane *tmp_plane;
	tmp_plane = calloc(num_planes, sizeof(*tmp_plane));

	while (1)
	{
		FD_ZERO (&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		r = select (fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r)
		{
			if (EINTR == errno)
				continue;
			printf ("select err\n");
		}
		if (0 == r)
		{
			fprintf (stderr, "select timeout\n");
			exit (EXIT_FAILURE);
		}

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.m.planes = tmp_plane;
		buf.length = num_planes;
		if (ioctl (fd, VIDIOC_DQBUF, &buf) < 0)
			printf("dqbuf fail\n");

		for (j = 0; j < num_planes; j++) {
			printf("plane[%d] start = %p, bytesused = %d\n", j, ((buffers + buf.index)->plane_start + j)->start, (tmp_plane + j)->bytesused);
			fwrite(((buffers + buf.index)->plane_start + j)->start, (tmp_plane + j)->bytesused, 1, file_fd);
		}

		num++;
		if(num >= atoi(argv[2]))
			break;

		if (ioctl (fd, VIDIOC_QBUF, &buf) < 0)
			printf("failture VIDIOC_QBUF\n");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
		printf("VIDIOC_STREAMOFF fail\n");

	free(tmp_plane);

	ret = 0;
unmmap:
err2:
	for (i = 0; i < req.count; i++) {
		for (j = 0; j < num_planes; j++) {
			if (MAP_FAILED != ((buffers + i)->plane_start + j)->start) {
				if (-1 == munmap(((buffers + i)->plane_start + j)->start, ((buffers + i)->planes_buffer + j)->length))
					printf ("munmap error\n");
			}
		}
	}

	for (i = 0; i < req.count; i++) {
		free((buffers + i)->planes_buffer);
		free((buffers + i)->plane_start);
	}

	free(buffers);

	fclose(file_fd);
err1:
	close(fd);
err:
	return ret;
}

