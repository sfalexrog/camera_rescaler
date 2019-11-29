#include <nodelet/nodelet.h>
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <pluginlib/class_list_macros.h>
#include <cv_bridge/cv_bridge.h>

#include <memory>

namespace camera_rescaler
{

class CameraRescalerNodelet: public nodelet::Nodelet
{
	private:

		image_transport::CameraPublisher pub_;
		image_transport::CameraSubscriber sub_;

		/* Negative numbers mean no rescaling */
		int target_width_ = -1;
		int target_height_ = -1;

		/* New camera_info that we're going to supply ourselves */
		sensor_msgs::CameraInfoPtr camera_info_;

	public:
		CameraRescalerNodelet() {}
		void onInit()
		{
			ros::NodeHandle& nh = getNodeHandle();
			ros::NodeHandle& nh_priv = getPrivateNodeHandle();

			image_transport::ImageTransport it(nh);
			image_transport::ImageTransport it_priv(nh_priv);

			target_width_ = nh_priv.param("target_width", -1);
			target_height_ = nh_priv.param("target_height", -1);

			camera_info_ = boost::make_shared<sensor_msgs::CameraInfo>();

			pub_ = it_priv.advertiseCamera("image_raw", 1);
			sub_ = it.subscribeCamera("image_raw", 1, &CameraRescalerNodelet::cameraCallback, this);
		}


		void cameraCallback(const sensor_msgs::ImageConstPtr& src, const sensor_msgs::CameraInfoConstPtr& cameraInfo)
		{
			const auto src_mat = cv_bridge::toCvShare(src, "bgr8");

			int width = (target_width_ == -1) ? src->width : target_width_;
			int height = (target_height_ == -1) ? src->height : target_height_;

			double aspect_x = double(width) / src->width;
			double aspect_y = double(height) / src->height;

			cv::Size size(width, height);

			cv::Mat dst;

			cv::resize(src_mat->image, dst, size);

			camera_info_->header.frame_id = cameraInfo->header.frame_id;
			camera_info_->header.stamp = cameraInfo->header.stamp;
			camera_info_->width = width;
			camera_info_->height = height;
			camera_info_->K = cameraInfo->K;
			camera_info_->D = cameraInfo->D;
			camera_info_->P = cameraInfo->P;
			camera_info_->R = cameraInfo->R;
			camera_info_->distortion_model = cameraInfo->distortion_model;
			camera_info_->binning_x = cameraInfo->binning_x;
			camera_info_->binning_y = cameraInfo->binning_y;
			camera_info_->roi = cameraInfo->roi;

			/* Rescale K and P */
			camera_info_->K[0] *= aspect_x;
			camera_info_->K[2] *= aspect_x;
			camera_info_->K[4] *= aspect_y;
			camera_info_->K[5] *= aspect_y;

			camera_info_->P[0] *= aspect_x;
			camera_info_->P[2] *= aspect_x;
			camera_info_->P[5] *= aspect_y;
			camera_info_->P[6] *= aspect_y;

			cv_bridge::CvImage output;
			output.image = dst;
			output.encoding = "bgr8";
			output.header.frame_id = src->header.frame_id;
			output.header.stamp = src->header.stamp;

			pub_.publish(output.toImageMsg(), camera_info_);
		}
};
}

PLUGINLIB_EXPORT_CLASS(camera_rescaler::CameraRescalerNodelet, nodelet::Nodelet);
