// Copyright 2022 Siddharth Saha
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#include "tensorrt_yolov7/node.hpp"

namespace perception
{
namespace tensorrt_yolov7_ros2
{

TensorRTYolov7Ros2Node::TensorRTYolov7Ros2Node(const rclcpp::NodeOptions & options)
:  rclcpp::Node("tensorrt_yolov7_ros2_node", options)
{
  engine_path_ = declare_parameter<std::string>("engine_path");
  debug_ = declare_parameter<int>("debug");
  frame_id_ = declare_parameter<std::string>("frame_id");
  yolov7_ = std::make_unique<Yolov7>(engine_path_);
  bgr_imgs_ = std::make_shared<std::vector<cv::Mat>>();
  bgr_imgs_->reserve(1);

  rmw_qos_profile_t qos_profile = rmw_qos_profile_sensor_data;
  auto sensor_msgs_qos = rclcpp::QoS(rclcpp::QoSInitialization(qos_profile.history, 1), qos_profile);

  input_image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "in/image", rclcpp::QoS{1},
      std::bind(&TensorRTYolov7Ros2Node::image_callback, this, std::placeholders::_1));
  
  detection_image_publisher_ = this->create_publisher<sensor_msgs::msg::Image>(
        "out/image",
        sensor_msgs_qos
  );

  objects_pub_ = this->create_publisher<tier4_perception_msgs::msg::DetectedObjectsWithFeature>(
        "out/objects",
        sensor_msgs_qos
  );

  params_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&TensorRTYolov7Ros2Node::param_callback, this, std::placeholders::_1));
  
}

rcl_interfaces::msg::SetParametersResult TensorRTYolov7Ros2Node::param_callback(
  const std::vector<rclcpp::Parameter> & parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = false;
  for (const auto & param : parameters) {
    if (param.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
      if (param.get_name() == "debug") {
        debug_ = param.as_int();
        result.successful = true;
      }
    } else if (param.get_type() == rclcpp::ParameterType::PARAMETER_STRING) {
      if (param.get_name() == "frame_id") {
        frame_id_ = param.as_string();
        result.successful = true;
      }
    }
  }
  result.reason = result.successful ? "success" : "failure";
  return result;
}

void TensorRTYolov7Ros2Node::image_callback(const sensor_msgs::msg::Image::SharedPtr msg)
{
  auto step_time = this->now();
  try {
    cv_ptr_ = cv_bridge::toCvCopy(msg, msg->encoding);
  } catch (cv_bridge::Exception& e) {
    RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    return;
  }

  if (bgr_imgs_->empty()) {
    bgr_imgs_->push_back(cv_ptr_->image);
  } else {
    bgr_imgs_->at(0) = cv_ptr_->image;
  }

  yolov7_->preProcess(*bgr_imgs_);

  yolov7_->infer();

  nmsresults_ = yolov7_->PostProcess();

  for(size_t i =0; i < nmsresults_.size();i++){
      // TODO: Publish here!
      Yolov7::DrawBoxesonGraph(bgr_imgs_->at(i),nmsresults_[i]);

      cv_ptr_->image = bgr_imgs_->at(i);
      detection_image_publisher_->publish(*(cv_ptr_->toImageMsg()).get() );
  }

  auto stop = now();
  auto diff = stop - step_time;
  if (debug_ > 0) {
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), debug_,"TensorRT Yolov7 Node took %f seconds", diff.seconds());
  }
}

}    // namespace tensorrt_yolov7_ros2
}  // namespace perception

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions options{};
  auto node = std::make_shared<perception::tensorrt_yolov7_ros2::TensorRTYolov7Ros2Node>(options);
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
