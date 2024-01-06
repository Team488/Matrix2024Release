#include "mrc.h"

using namespace cv;
using namespace std;

int thresh = 19;
int max_thresh = 255;
int dist = 3000;
int max_dist = 10000;

int main(int argc, char** argv)
{
	if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
	{
		std::cout << "Usage: ./mrc [-v|--view] [-s|--stream IP PORT] [[COLOR_MAT_NAME] [DEPTH_MAT_NAME]]" << std::endl;
		return 0;
	}

	bool isViewing = false;
	bool isStreaming = false;
	std::string ip;
	std::string port;
	cv::VideoWriter writer;
	char *colorMatName = "BALLCAM_COLOR_0";
    char *depthMatName = "BALLCAM_DEPTH_0";
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--view") == 0)
		{
			isViewing = true;
		}
		else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stream") == 0)
		{
			isStreaming = true;
			ip = std::string(argv[++i]);
			port = std::string(argv[++i]);
		}
		else
		{
			//UGLY HACK
            colorMatName = argv[i];
            i++;
            depthMatName = argv[i];
		}
	}

	string rv_name = "RGB Video";
    string dv_name = "Depth Video";
	string bv_name = "Ball View";
	string cv_name = "Canny View";
	if (isViewing)
	{
		namedWindow(rv_name);
		namedWindow(dv_name);
		createTrackbar(" Max Distance:", dv_name, &dist, max_dist);
		//namedWindow(bv_name);
		namedWindow(cv_name);
		createTrackbar(" Canny thresh:", cv_name, &thresh, max_thresh);
	}

	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
    vector<vector<Point> > contours_poly(1);
    vector<Point2f>center(1);
    vector<float>radius(1);

	vector<Vec3f> circles;

	Mat ball_image;
	Mat src_gray, out_frame, con_frame;
	Mat depth_blur, canny_depth, depth_mask, masked_depth, depth_norm, depth_correct;



	SharedMat color_mat(colorMatName);
    SharedMat depth_mat(depthMatName);

	bool isFirstFrame = true;
	///TODO: Add alignment call or loading the settings files

	//Test Params
	// SimpleBlobDetector::Params params;
	// params.filterByArea = true;
	// params.minArea = 100;
	// params.filterByCircularity = true;
	// params.minCircularity = 0.9;
	// params.filterByConvexity = true;
	// params.minConvexity = 0.2;
	// params.filterByInertia = true;
	// params.minInertiaRatio = 0.01;

	// Ptr<SimpleBlobDetector> SBD = SimpleBlobDetector::create(params);
	// vector<KeyPoint> keypoints;

	cout << "Starting Loop" << endl;

	while (true)
	{
		int frameColorNum = color_mat.waitForFrame();
        int frameDepthNum = depth_mat.waitForFrame();
		Mat cSrc = color_mat.mat;
        Mat dSrc = depth_mat.mat;

		cv::Size size = cSrc.size();
		if (isFirstFrame)
		{
			isFirstFrame = false;
			std::cout << "Resolution is: " << size.width << " x " << size.height << std::endl;
			
			if (isStreaming)
			{
				const cv::String output_pipeline = "appsrc ! video/x-raw, format=(string)BGR, width=(int)" + to_string(size.width) + ", height=(int)" + to_string(size.height) + ", framerate=(fraction)60/1 ! videoconvert ! omxh264enc quality-level=0 control-rate=2 bitrate=400000 ! video/x-h264, stream-format=(string)byte-stream ! h264parse ! rtph264pay ! udpsink host=" + ip + " port=" + port;
				const int codec = VideoWriter::fourcc('X','2','6','4');
				writer.open(output_pipeline, codec, 60, size);
			}
		}

		GaussianBlur(cSrc, out_frame, Size(11, 11), 0, 0);
		GaussianBlur(dSrc, depth_blur, Size(7, 7), 1.5, 1.5);
		threshold(depth_blur, depth_correct, 29, 65535, THRESH_TOZERO);
		threshold(depth_correct, depth_mask, dist, 65535, THRESH_BINARY_INV);
		bitwise_and(depth_blur, depth_mask, masked_depth);
		
		// double min, max;
		// minMaxIdx(masked_depth, &min, &max);
		// printf("masked depth - min: %f, max: %f\n", min, max);
		//masked_depth.convertTo(depth_norm, CV_8U);

		//normalize(depth_correct, depth_norm, 0.,255.,cv::NORM_MINMAX, CV_8U);
		//normalize(masked_depth,depth_norm,0.,255.,cv::NORM_MINMAX,CV_8U);
		// minMaxIdx(depth_norm, &min, &max);
		// printf("depth_norm - min: %f, max: %f\n", min, max);

		masked_depth = (255.0/dist)*masked_depth;
		masked_depth.convertTo(depth_norm, CV_8U);
		
		//cvtColor(out_frame, out_frame, COLOR_BGR2HSV);
		Canny(depth_norm, canny_depth, thresh, thresh*2, 3);
		
		//SBD->detect(depth_norm, keypoints);

		//drawKeypoints( cSrc, keypoints, cSrc, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );

        //inRange(out_frame, Scalar(110, 50, 100), Scalar(130, 255, 255), out_frame);
        //erode(out_frame, out_frame, 2);
        //dilate(out_frame, out_frame, 2);

		HoughCircles(depth_norm, circles, HOUGH_GRADIENT, 1.5, depth_norm.rows/4, thresh*2, 20 );
		for( size_t i = 0; i < circles.size(); i++ )
		{
			Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
			int radius = cvRound(circles[i][2]);
			// draw the circle center
			circle( cSrc, center, 3, Scalar(0,255,0), -1, 8, 0 );
			// draw the circle outline
			circle( cSrc, center, radius, Scalar(0,0,255), 3, 8, 0 );
		}

        // vector<vector<Point> > contours;
        // vector<Vec4i> hierarchy;
        // findContours(out_frame, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		// con_frame = out_frame;

        // if (contours.size() != 0) {
        //     //Drawing will be what we run calculations on, Display is for debugging/viewing
        //     for(size_t i = 0; i< contours.size(); i++) {
        //         if (contourArea(contours[i]) > 20.0) {  //No ball contour should be smaller than __ pixels.
        //             vector<Point> ballContour = contours[i];
        //             Rect r = boundingRect(ballContour);

        //             vector<Moments> mu(1);
        //             mu[0] = moments(ballContour, true);
        //             Moments M = mu[0];
        //             minEnclosingCircle( ballContour, center[0], radius[0] );
        //             circle( con_frame, center[0], (int)radius[0], 255, 1, 8, 0 ); //Viewer display.
        //         }
        //     }

        //     //imshow("Contours", con_frame);
        // }


		/// Find contours
		//findContours(canny_output, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

		/// Draw contours
		//drawing = Mat(canny_output.size(), CV_8UC3, Scalar(255,255,255));//Mat::zeros(canny_output.size(), CV_8UC3);
		//drawContours(drawing, contours, -1, Scalar(0,0,0), 1);

		// Draw Lines
		// line(drawing, Point(size.width/2, 0), Point(size.width/2, size.height), Scalar(255,0,0), 2);
		// line(drawing, Point(size.width/4, size.height/3), Point(size.width/4, 2*size.height/3), Scalar(0,255,0), 2);
		// line(drawing, Point(3*size.width/4, size.height/3), Point(3*size.width/4, 2*size.height/3), Scalar(0,255,0), 2);

		// for (int i = 0; i < contours.size(); i++)
		// {
		// 	Scalar color = Scalar(0,0,0);//Scalar(255, 255, 255);//Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
		// 	drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
		// }

		//show the frame in the created window
		if (isViewing)
		{
			imshow(rv_name, cSrc);
			imshow(dv_name, depth_norm);
			imshow(cv_name, canny_depth);
			imshow("Depth Mask", depth_mask);
			imshow("Masked Depth", masked_depth);
		}

		if (isStreaming)
		{
			writer.write(cSrc);
		}

		//wait for for 1 ms until any key is pressed.  
		//If the 'Esc' key is pressed, break the while loop.
		//If the any other key is pressed, continue the loop 
		//If any key is not pressed withing 1 ms, continue the loop 
		if (waitKey(1) == 27)
		{
			std::cout << "Esc key is pressed by user. Stopping the video" << std::endl;
			break;
		}
	}

	return 0;
}