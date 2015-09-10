//这是一个验证码识别类，提供了多种验证码图片处理策略。 每个策略方法可以提供不同与默认参数的其他参数，多个方法也可以自由组合，以实现更好的图片处理效果。
//部分图像处理基于opencv，提取出的字符图片校正后，交由tesseract-ocr识别。
//do_recognize方法是一个自带的demo，组合了灰度化、自动阈值分割、滤波、去边框、字符分割、字符校正、字符重组等策略，然后将处理好的图片交给ocr进行识别。
//使用者可以根据图片的特性，选择合适的策略，将图片上的文字最大清晰化。
//提供截屏功能。
//对前景与背景对比度大、不粘连的英文，分割与校正的处理效果比较好；但字符粘连的，由于时间比较紧，还没提供粘连字符分割的方法，后续会完善。
//中文的需要修改类中的ocr方法，以切换为中文识别。由于验证码中的中文字符很多是变形的，即使能用各种方法将字符分离出来，
//但tesseract-ocr提供的中文字库，对变形的中文识别率很低，所以要根据待处理的验证码字符特征，训练ocr字库，以提高准确率。
//后续，会继续完善图片处理策略，以增强字符图片稳正度，提高tesseract ocr识别率。

#pragma once

#pragma warning(push)
#pragma warning(disable:4819)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#ifdef _DEBUG
	#pragma comment(lib,"opencv_world300d.lib")
	#pragma  comment(lib,"libtesseract302d.lib")
#else
	#pragma comment(lib,"opencv_world300.lib")
	#pragma  comment(lib,"libtesseract302.lib")
#endif

#include "strngs.h"
#include "baseapi.h"

#include "../based/headers_all.h"

#ifdef DEBUG_SHOW_IMG
	#define SHOW_IMG(IMG_WINDOW_NAME,IMG_NAME)		cvShowImage(IMG_WINDOW_NAME, IMG_NAME)
#else
	#define SHOW_IMG(IMG_WINDOW_NAME,IMG_NAME)
#endif


class img_recognize
{
public:
	//验证码图像识别：filename：验证码图片的路径; char_count_needed需要识别的字符个数; method:true,分割后的字符图片校正后直接识别,false重新组合为整图再识别; debug_mode:是否启用诊断模式
	//可以组合各个方法，以实现不同的图片处理流程；也可以为每个方法提供不同于默认参数的其他参数，以实现更好的处理效果。
	virtual std::string do_recognize(char* filename, int char_count_needed = 4, bool recg_one_by_one = false, bool debug_mode = true)
	{
		std::string final_str;

		//加载图像
		IplImage *img = cvLoadImage(filename);
		ENSURE(img && img->imageData)(img).warn();
		SCOPE_GUARD([&] {cvReleaseImage(&img);});

		//转灰度图
		IplImage* img_gray = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
		SCOPE_GUARD([&] {cvReleaseImage(&img_gray);});
		cvCvtColor(img, img_gray, CV_BGR2GRAY);	
		SHOW_IMG("after gray", img_gray);

		//自动阈值分割出二值图像
		threshold_otsu(img_gray,2);
		SHOW_IMG("after threshold", img_gray);

		//中值滤波去噪
		cv::Mat mat_blur = cv::cvarrToMat(img_gray);
		//medianBlur(mat_blur, mat_blur, 5);
		SHOW_IMG("after blur", img_gray);

		//去边框
		move_border(img_gray);
		SHOW_IMG("after move border", img_gray);

		//字符分割
		IplImage** splitted_imgs = split_char_by_shadow(img_gray, char_count_needed);
		SCOPE_GUARD([&] {
			for (int i = 0;i < char_count_needed;i++)
				cvReleaseImage(splitted_imgs+i);
			delete[] splitted_imgs;
		});
		if (char_count_needed == 0)
			return "";

		//对分割出的每个字符图像，做校正并逐个识别，或校正后重组为完整的图像并一次性识别
		IplImage** rectified_imgs = new IplImage*[char_count_needed];
		for (int i = 0;i < char_count_needed;i++)
		{
			SHOW_IMG((std::string("before rotate rectify ") + std::to_string(i)).c_str(), splitted_imgs[i]);
			rectified_imgs[i] = rectify_rotate_char_img(splitted_imgs[i]);
			SHOW_IMG((std::string("after rotate rectify ") + std::to_string(i)).c_str(), rectified_imgs[i]);

			if (recg_one_by_one)
			{
				char* dst_path = "f:\\aaa.png";
				cvSaveImage(dst_path, rectified_imgs[i]);
				final_str += ocr(dst_path);
			}
		}
		SCOPE_GUARD([&] {
			for (int i = 0;i < char_count_needed;i++)
				cvReleaseImage(rectified_imgs+i);
			delete[] rectified_imgs;
		});

		if (!recg_one_by_one)
		{
			IplImage* dst = combine_char_image(rectified_imgs, char_count_needed);
			SCOPE_GUARD([&] {cvReleaseImage(&dst);});

			SHOW_IMG("final img", dst);
			char* dst_path = "f:\\aaa.png";
			cvSaveImage(dst_path, dst);
			final_str = ocr(dst_path);
		}
		
		return final_str;
	}

	//截屏，保存截取的图片到save_full_path（需带扩展名）中。start_x,start_y要截取的起始坐标，end_x，end_y要截取的终止坐标。坐标都是相对于屏幕左上角（左上角坐标0，0）。
	void copy_screen(const char* save_full_path, int start_x, int start_y, int end_x, int end_y)
	{
	#ifdef WIN32
		HDC dc_screen = ::GetDC(NULL);
		ENSURE_WIN32(dc_screen != NULL).warn();
		SCOPE_GUARD([&] { ReleaseDC(NULL,dc_screen); });

		int screen_width = GetDeviceCaps(dc_screen, HORZRES);
		int screen_height = GetDeviceCaps(dc_screen, VERTRES);
		start_x < 0 ? start_x = 0 : void();
		start_y < 0 ? start_y = 0 : void();
		end_x > screen_width ? end_x = screen_width : void();
		end_y > screen_height ? end_y = screen_height : void();

		HDC dc_mem = CreateCompatibleDC(dc_screen);
		SCOPE_GUARD([&] { DeleteDC(dc_mem); });

		HBITMAP comp_bmp = CreateCompatibleBitmap(dc_screen, end_x - start_x + 1, end_y - start_y + 1);
		SCOPE_GUARD([&] { DeleteObject(comp_bmp); });

		HBITMAP old_bmp = (HBITMAP)SelectObject(dc_mem, comp_bmp);
		BitBlt(dc_mem, 0, 0, end_x - start_x + 1, end_y - start_y + 1, dc_screen, start_x, start_y, SRCCOPY);
		comp_bmp = (HBITMAP)SelectObject(dc_mem, old_bmp);

		BITMAPINFOHEADER bih{};
		bih.biSize = sizeof(BITMAPINFOHEADER);
		bih.biWidth = end_x - start_x + 1;
		bih.biHeight = end_y - start_y + 1;
		bih.biPlanes = 1;
		bih.biBitCount = 32;
		bih.biCompression = BI_RGB;
		int line_bytes = (bih.biWidth*bih.biBitCount + bih.biBitCount-1)/ bih.biBitCount * 4;
		char* bmp_data = new char[line_bytes*bih.biHeight];
		SCOPE_GUARD([&] { delete[] bmp_data; });

		ENSURE_WIN32(GetDIBits(dc_mem, comp_bmp, 0, bih.biHeight, bmp_data, (BITMAPINFO*)&bih, DIB_RGB_COLORS) != 0).warn();

		IplImage* dst = cvCreateImage(cvSize(bih.biWidth,bih.biHeight),IPL_DEPTH_8U,4);
		SCOPE_GUARD([&] { cvReleaseImage(&dst); });

		for (int i = 0;i < bih.biHeight;i++)
			for (int j = 0;j < bih.biWidth;j++)
			{
				dst->imageData[i*line_bytes + j * 4 + 0] = bmp_data[(bih.biHeight-i-1)*line_bytes + j * 4 + 0];
				dst->imageData[i*line_bytes + j * 4 + 1] = bmp_data[(bih.biHeight-i-1)*line_bytes + j * 4 + 1];
				dst->imageData[i*line_bytes + j * 4 + 2] = bmp_data[(bih.biHeight-i-1)*line_bytes + j * 4 + 2];
				dst->imageData[i*line_bytes + j * 4 + 3] = bmp_data[(bih.biHeight-i-1)*line_bytes + j * 4 + 3];
			}

		std::string str_path = save_full_path;
		std::replace(str_path.begin(), str_path.end(), '/', '\\');
		str_path.erase(str_path.rfind('\\'));
		based::os::make_dir_recursive(str_path.c_str());
		cvSaveImage(save_full_path, dst);
	#else
		#error "Please add corresponding platform's code."
	#endif
	}

	//截屏，保存到save_full_path（需带扩展名）中。start_x,start_y要截取的起始坐标，width，height要截取的宽度和高度。坐标相对于屏幕左上角（左上角坐标0，0）。
	void copy_screen(const char* save_full_path, int start_x, int start_y, short width, short height)
	{
		copy_screen(save_full_path, start_x, start_y, start_x + width, start_y + height);
	}

protected:
	//去边框：src:256色单通道灰度图, x：x方向 y：y方向
	void move_border(IplImage* src, int x = 2, int y = 2)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		if (src->width < y || src->height < x)
			return;

		int width = src->width;
		int height = src->height;
		int line_bytes = (width*src->nChannels + 3) / 4 * 4;
		char* src_data = src->imageData;

		for (int i = 0;i < height;i++)
			for (int j = 0;j < width;j++)
			{
				if (!(i >= y && i <= height - y && j >= x && j <= width - x))
					src_data[i*line_bytes + j] = (char)0xff;
			}
	}

	//使用增加放大因子后的otsu算法，寻找最佳阈值，并以此阈值做二值化处理：src:256色单通道灰度图 boost_scale:放大因子(1--n)，默认3, 返回最佳阈值
	int threshold_otsu(IplImage* src, double boost_scale = 3)
	{
		ENSURE(src->nChannels == 1 && boost_scale > 0)(src->nChannels)(boost_scale).warn();

		unsigned int h[256]{};
		double g[256]{};
		int height = src->height;
		int width = src->width;
		int line_bytes = (width * src->nChannels + 3) / 4 * 4;
		unsigned char* src_data = (unsigned char*)src->imageData;

		for (int i = 0; i < height; i++)
			for (int j = 0; j < width; j++)
			h[*(src_data + i*line_bytes + j)]++;

		for (int i = 0;i < 256;i++)
		{
			double fore_count = 0, bkg_count = 0;
			double u0 = 0, u1 = 0, u = 0;
			for (int j = 0;j < 256;j++)
			{
				if (j < i)
				{
					fore_count += h[j] * boost_scale;		//在otsu基础上放大3倍
					u0 += j*h[j];
				}
				else
				{
					bkg_count += h[j] * boost_scale;		//在otsu基础上放大3倍
					u1 += j*h[j];
				}
			}

			if (fore_count != 0)
				u0 /= fore_count;
			double w0 = fore_count / (height*width);

			if (bkg_count != 0)
				u1 /= bkg_count;
			double w1 = 1 - w0;

			g[i] = w0*w1*(u1 - u0)*(u1 - u0);
		}

		int idx = 0;
		double gmax = 0;
		for (int i = 0; i<256; i++)
			if (g[i]>gmax)
			{
				gmax = g[i];
				idx = i;
			}

		for (int i = 0; i < height; i++)
			for (int j = 0; j<width; j++)
			*(src_data + i*line_bytes + j) = *(src_data + i*line_bytes + j)>idx ? 0xff : 0;

		return idx;
	}

	//削弱背景离散点：src:256色单通道灰度图, scale:窗口因子大小(1 3 5 7)，必须是奇数, threshold:强度(0.0--1.0)
	//处理背景时：计算点x,y为中心宽度为scale的窗口的平均像素值，如果不足threshold，则认为是离散噪点，减弱为白色；
	void move_discrete_point(IplImage* src, int scale, double threshold)
	{
		ENSURE(src->nChannels == 1 && scale > 0 && scale % 2 == 1)(src->nChannels)(scale).warn();

		if (src->width < scale || src->height < scale)
			return;

		char* src_data = src->imageData;
		int height = src->height;
		int width = src->width;
		int line_bytes = (width * src->nChannels + 3) / 4 * 4;
		scale = (scale - 1) / 2;
		unsigned char* dst_data = new unsigned char[line_bytes*height];
		SCOPE_GUARD([&] { delete[] dst_data; });

		for (int i = 0;i < height;i++)
			for (int j = 0;j < width;j++)
			dst_data[i*line_bytes + j] = 0xff;

		for (int i = scale;i < height - scale;i++)
			for (int j = scale;j < width - scale;j++)
			{
				double scale_value_sum = 0;
				for (int k = i - scale;k <= i + scale;k++)
					for (int n = j - scale;n <= j + scale;n++)
					scale_value_sum += (unsigned char)(0xff - src_data[k*line_bytes + n]);

				if (scale_value_sum < ((double)((unsigned char)((unsigned char)0xff - src_data[i*line_bytes + j]) * (scale * 2 + 1)*(scale * 2 + 1)))*threshold)
					dst_data[i*line_bytes + j] = 0xff;
				else
					dst_data[i*line_bytes + j] = src_data[i*line_bytes + j];
			}

		for (int i = scale;i < height - scale;i++)
			for (int j = scale;j < width - scale;j++)
			src_data[i*line_bytes + j] = dst_data[i*line_bytes + j];
	}

	//计算图像在x轴的投影，黑色像素有影子：src:二值图像; buf_len:传出投影缓冲的长度; 返回值为投影缓冲区地址，需调用者释放
	char* shadow_x(IplImage* src, int& buf_len)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		int width = src->width;
		int height = src->height;
		char* data = src->imageData;
		int line_bytes = (width * src->nChannels + 3) / 4 * 4;

		buf_len = width;
		char* buf_shadow = new char[buf_len];
		for (int i = 0;i < height;i++)
		{
			for (int j = 0;j < width;j++)
			{
				if ((unsigned char)*(data + i*line_bytes + j) == (unsigned char)0)
					buf_shadow[j] = 1;
			}
		}

		return buf_shadow;
	}

	//投影法分割字符，src:256色单通道灰度图, dst_char_count:传入想要取得的字符个数，传出实际分割出的字符的个数。 返回分割好的图片数组指针，需调用者释放
	IplImage** split_char_by_shadow(IplImage* src, int& dst_char_count)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		int real_char_count = 0;
		int height = src->height;
		int width = src->width;
		int line_bytes = (width * src->nChannels + 3) / 4 * 4;
		char* src_data = src->imageData;

		struct character
		{
			int begin;
			int end;
		};

		character* real_chars = new character[width]{};	
		char* first_line = shadow_x(src, width);
		SCOPE_GUARD([&] { delete[] real_chars; });
		SCOPE_GUARD([&] {delete[] first_line;});

		bool continous = false;
		for (int i = 0;i < width;i++)
		{
			if (first_line[i] == 1)
			{
				if (!continous)
				{
					real_char_count++;
					real_chars[real_char_count - 1].begin = i;
					continous = true;
				}
			}
			else
			{
				if (continous)
				{
					real_chars[real_char_count - 1].end = i - 1;
					continous = false;
				}
			}
		}
		if (real_chars[real_char_count - 1].end == 0)
			if (--real_char_count < 0)
			{
				real_char_count = 0;
				return NULL;
			}
		if (real_char_count < dst_char_count)
			dst_char_count = real_char_count;

		character* dst_chars = new character[dst_char_count];
		SCOPE_GUARD([&] { delete[] dst_chars; });

		for (int i = 0;i < dst_char_count;i++)
			dst_chars[i] = real_chars[i];

		for (int i = dst_char_count;i < real_char_count;i++)
		{
			int min_width = real_chars[i].end - real_chars[i].begin;
			int min_index = i;
			for (int j = 0;j < dst_char_count;j++)
			{
				if (dst_chars[j].end - dst_chars[j].begin < min_width)
				{
					min_width = dst_chars[j].end - dst_chars[j].begin;
					min_index = j;
				}
			}
			if (min_index == i)
				continue;

			for (int n = min_index + 1;n < dst_char_count;n++)
				dst_chars[n-1] = dst_chars[n];
			dst_chars[dst_char_count-1] = real_chars[i];
		}

		IplImage** dst_img = new IplImage*[dst_char_count];
		for (int i = 0;i < dst_char_count;i++)
		{
			int dst_img_width = dst_chars[i].end - dst_chars[i].begin + 1;
			int dst_img_line_bytes = (dst_img_width*src->nChannels + 3) / 4 * 4;
			dst_img[i] = cvCreateImage(cvSize(dst_img_width, height), src->depth, src->nChannels);
			char* dst_img_data = dst_img[i]->imageData;

			for (int j = 0;j < height;j++)
				for (int k = 0;k < dst_img_width;k++)
				{
					dst_img_data[j*dst_img_line_bytes + k] = src_data[j*line_bytes + dst_chars[i].begin + k];
				}
		}

		return dst_img;
	}

	//旋转图像：src:256色单通道灰度图, angle:旋转角度，正数为逆时针，负数为顺时针, 返回旋转后的图像，需要调用者释放
	IplImage* rotate_image(IplImage* src, int angle)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		int positive_angle = abs(angle) % 180;
		if (positive_angle > 90)
			positive_angle = 90 - (positive_angle % 90);

		double dst_width;
		double dst_height = src->height;
		get_height_width_after_rotate(src,angle,NULL, &dst_width);
		if (dst_width - (int)dst_width > 0.000001)
			dst_width = (int)dst_width + 1;

		int tempLength = (int)sqrt((double)src->width * src->width + src->height * src->height) + 10;
		int tempX = (int)((tempLength + 1) / 2.0 - src->width / 2.0);
		int tempY = (int)((tempLength + 1) / 2.0 - src->height / 2.0);

		IplImage* dst = cvCreateImage(cvSize((int)(dst_width+4), (int)dst_height), src->depth, src->nChannels);
		cvSet(dst, 0xff);
		IplImage* temp = cvCreateImage(cvSize(tempLength, tempLength), src->depth, src->nChannels);
		cvSet(temp, 0xff);

		cvSetImageROI(temp, cvRect(tempX, tempY, src->width, src->height));
		cvCopy(src, temp, NULL);
		cvResetImageROI(temp);

		float m[6];
		m[0] = (float)cos(-angle * CV_PI / 180.);
		m[1] = (float)sin(-angle * CV_PI / 180.);
		m[3] = -m[1];
		m[4] = m[0];
		m[2] = temp->width * 0.5f;
		m[5] = temp->height * 0.5f;

		CvMat M = cvMat(2, 3, CV_32F, m);
		cvGetQuadrangleSubPix(temp, dst, &M);
		cvReleaseImage(&temp);

		return dst;
	}

	//计算图像旋转angle角度后的宽和高：src:二值图像; angle正数为逆时针旋转，负数为顺时针旋转。如果new_height或new_width不为NULL,则将宽高保存至new_height或new_width。
	void get_height_width_after_rotate(IplImage* src, int angle,double* new_height,double* new_width)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		int height = src->height;
		int width = src->width;
		int line_bytes = (width*src->nChannels + 3) / 4 * 4;
		char* src_data = src->imageData;

		double min_x = std::numeric_limits<double>::max();
		double max_x = std::numeric_limits<double>::min();
		double min_y = std::numeric_limits<double>::max();
		double max_y = std::numeric_limits<double>::min();

		for (int j = 0;j < height;j++)
		{
			for (int k = 0;k < width;k++)
			{
				if (src_data[j*line_bytes + k] == 0)
				{
					double pix_new_x = (k - width*0.5f)*cos(angle / 180. * CV_PI) - (height*0.5f - j)*sin(angle / 180. * CV_PI);
					double pix_new_y = (k - width*0.5f)*sin(angle / 180. * CV_PI) + (height*0.5f - j)*cos(angle / 180. * CV_PI);
					if (pix_new_x < min_x)
						min_x = pix_new_x;
					if (pix_new_x > max_x)
						max_x = pix_new_x;
					if (pix_new_y < min_y)
						min_y = pix_new_y;
					if (pix_new_y > max_y)
						max_y = pix_new_y;
				}
			}
		}
		if(new_width != NULL)
			*new_width = max_x - min_x;
		if(new_height != NULL)
			*new_height = max_y - min_y;
	}

	//对字符图像做旋转校正，src:256色单通道灰度图，max_angle_left最大左旋角度，max_angle_right最大右旋角度，返回最稳正的图像，需调用者释放
	IplImage* rectify_rotate_char_img(IplImage* src, int max_angle_left = -20, int max_angle_right = 20)
	{
		ENSURE(src->nChannels == 1)(src->nChannels).warn();

		int height = src->height;
		int width = src->width;
		int line_bytes = (width*src->nChannels + 3) / 4 * 4;
		char* src_data = src->imageData;
		double min_diagonal_len_2 = std::numeric_limits<double>::max();
		int right_angle = 0;

		for (int i = max_angle_left; i <= max_angle_right; i += 1)
		{
			double new_height;
			double new_width;
			get_height_width_after_rotate(src,i,&new_height,&new_width);
			
			double diagonal_len_2 = new_width*new_width + new_height*new_height;
			if (diagonal_len_2 < min_diagonal_len_2)
			{
				min_diagonal_len_2 = diagonal_len_2;
				right_angle = i;
			}
		}

		if (right_angle == 0)
		{
			IplImage* dst = cvCreateImage(cvSize(src->width+2, src->height+2), src->depth, src->nChannels);
			cvSet(dst,0xff);
			cvSetImageROI(dst,cvRect(1,1,src->width,src->height));
			cvCopy(src,dst);
			cvResetImageROI(dst);
			return dst;
		}
		return rotate_image(src, right_angle);
	}

	//重新组合单个字符图像:src_char_img:要组合的字符图像指针数组，dst_char_count:图像的个数，fill_color：空隙的填充色，space_x:x方向间隙,space_y:y方向间隙，返回组合完的图像，需调用者释放
	IplImage* combine_char_image(IplImage** src_char_img, int dst_char_count, unsigned char fill_color = 0xff, int space_x = 4, int space_y = 1)
	{
		ENSURE(dst_char_count>0)(dst_char_count).warn();

		int height = -1;
		int width = (dst_char_count + 1)*space_x;
		for (int i = 0;i < dst_char_count;i++)
		{
			width += src_char_img[i]->width;
			if (src_char_img[i]->height > height)
				height = src_char_img[i]->height;
		}
		height += (space_y * 2);

		IplImage* dst = cvCreateImage(cvSize(width, height), src_char_img[0]->depth, src_char_img[0]->nChannels);
		cvSet(dst, fill_color);

		int roi_x = space_x;
		int roi_y = space_y;
		for (int i = 0;i < dst_char_count;i++)
		{
			cvSetImageROI(dst, cvRect(roi_x, roi_y, src_char_img[i]->width, src_char_img[i]->height));
			cvCopy(src_char_img[i], dst);
			cvResetImageROI(dst);
			roi_x += (src_char_img[i]->width + space_x);
		}

		return dst;
	}

	//对img_path对应的图片进行ocr识别, method:识别方式, 返回识别后的字符串
	virtual std::string ocr(const char* img_path, int method = 0)
	{
		tesseract::TessBaseAPI  api;

		//设置语言库，中文简体：chi_sim;英文：eng；也可以自己训练语言库
		api.Init(NULL, "eng", tesseract::OEM_DEFAULT);
		api.SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

		STRING text_out = "";
		if (!api.ProcessPages(img_path, NULL, 0, &text_out))
		{
			std::cout << "ocr exception" << std::endl;
		}

		std::string str_text_u8 = text_out.string();
		std::string str_text = based::charset_convert().utf8_string_to_string(str_text_u8);
		based::string_more::replace_all<std::string>(str_text, "\n", "");

		return str_text;
	}
};

// example:
// #include "img_recognize.h"
// int main(int argc, char* argv[])
// {
// 	std::cout << "Please get ready,then press enter." << std::endl;
// 	getchar();
// 
//  //验证码在屏幕中的起始位置（相对与屏幕左上角，其中，左上角坐标为0，0）：600,374，宽度为413，高度为120
// 	std::string& screen_save_path = based::os::get_exe_path_without_backslash<char>() + "\\copy_screen.bmp";
// 	img_recognize().copy_screen(screen_save_path.c_str(),600, 374, (short)413, (short)120);
// 	std::cout << img_recognize().do_recognize((char*)screen_save_path.c_str()) << std::endl;
// 
// 	cvWaitKey(0);
// 
// 	getchar();
// 	return 0;
// }