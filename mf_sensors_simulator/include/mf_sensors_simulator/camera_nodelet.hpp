/**
 * @file
 *
 * \brief  Declaration of a nodelet simulating a camera
 * \author Corentin Chauvin-Hameau
 * \date   2019
 */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "mf_sensors_simulator/CameraOutput.h"
#include "mf_farm_simulator/rviz_visualisation.hpp"
#include "mf_farm_simulator/Algae.h"
#include "reactphysics3d.h"
#include <tf2_ros/transform_listener.h>
#include <tf2/LinearMath/Vector3.h>
#include <geometry_msgs/TransformStamped.h>
#include <nodelet/nodelet.h>
#include <ros/ros.h>
#include <csignal>
#include <string>
#include <vector>


namespace mfcpp {

typedef std::unique_ptr<rp3d::BoxShape> box_shape_ptr;


/**
 * \brief  Nodelet for a simulated camera
 *
 * For each pixel, the simulated sensor casts a ray. If the ray touches an alga,
 * the sensor will return the hit position, and the corresponding value of
 * the alga disease heatmap.
 *
 * To improve performance, two collision worlds are created:
 * - a big collision world containing all the algae and the field of view (FOV)
 * of the camera.
 * - a small one only for raycasting.
 *
 * A first step is then to test overlap between the camera FOV and the algae, the
 * overlapping algae populates the small collision world. Therefore, raycasting
 * is performed on a smaller amount of algae.
 */
class CameraNodelet: public nodelet::Nodelet {
  public:
    CameraNodelet();
    ~CameraNodelet();

    /**
     * \brief  Function called at beginning of nodelet execution
     */
    virtual void onInit();

  private:
    /**
     * \brief  Callback class for raycasting
     */
    class RaycastCallback: public rp3d::RaycastCallback
    {
      public:
        RaycastCallback(CameraNodelet *parent);
        virtual rp3d::decimal notifyRaycastHit(const rp3d::RaycastInfo& info);

        bool alga_hit_;         ///<  Whether an alga has been hit
        tf2::Vector3 hit_pt_;   ///<  Hit point
        int alga_idx_;          ///<  Index of the hit alga in the ray_bodies_ vector
      private:
        CameraNodelet *parent_;  ///<  Parent CameraNodelet instance
    };

    /**
     * \brief  Callback class for overlap detection
     */
    class OverlapCallback: public rp3d::OverlapCallback
    {
      public:
        OverlapCallback(CameraNodelet *parent);
        virtual void notifyOverlap(rp3d::CollisionBody *body);

      private:
        /// Parent CameraNodelet instance
        CameraNodelet *parent_;
    };


    // Static members
    // Note: the timers need to be static since stopped by the SIGINT callback
    static sig_atomic_t volatile b_sigint_;  ///<  Whether SIGINT signal has been received
    static ros::Timer main_timer_;   ///<  Timer callback for the main function

    // Private members
    ros::NodeHandle nh_;          ///<  Node handler (for topics and services)
    ros::NodeHandle private_nh_;  ///<  Private node handler (for parameters)
    ros::Subscriber algae_sub_;   ///<  Subscriber for the algae of the farm
    ros::Publisher out_pub_;      ///<  Publisher for the camera output
    ros::Publisher rviz_pub_;     ///<  Publisher for Rviz markers
    tf2_ros::Buffer tf_buffer_;
    tf2_ros::TransformListener tf_listener_;

    mf_farm_simulator::AlgaeConstPtr last_algae_msg_;  ///<  Last algae message
    bool algae_msg_received_;  ///<  Whether an algae message has been received
    geometry_msgs::TransformStamped fixed_camera_tf_;  ///<  Transform from fixed frame to camera
    geometry_msgs::TransformStamped camera_fixed_tf_;  ///<  Transform from camera to fixed frame
    std::vector<std::vector<std::vector<float>>> heatmaps_;  ///<  Disease heatmatps for all the algae
    std::vector<int> corr_algae_;  ///<  Correspondance between the algae used for raytracing and all the others

    /// \name  Collision members
    ///@{
    bool world_init_;   ///<  Whether the collision world has been initialised
    rp3d::CollisionWorld coll_world_;     ///<  World with all algae and camera FOV
    rp3d::CollisionWorld ray_world_;      ///<  World with algae colliding with FOV
    rp3d::WorldSettings world_settings_;  ///<  Collision world settings
    std::vector<rp3d::CollisionBody*> algae_bodies_;  ///<  Collision bodies of the algae
    std::vector<rp3d::CollisionBody*> ray_bodies_;    ///<  Collision bodies for raycasting
    rp3d::CollisionBody* fov_body_;            ///<  Collision body of the camera FOV
    std::vector<box_shape_ptr> algae_shapes_;  ///<  Collision shapes of the algae bodies
    std::vector<box_shape_ptr> ray_shapes_;    ///<  Collision shapes of algae for raycasting
    box_shape_ptr fov_shape_;  ///<  Collision shapes of the FOV body

    RaycastCallback raycast_cb_;  ///<  Callback instance for raycasting
    OverlapCallback overlap_cb_;  ///<  Callback instance for overlap detection
    ///@}


    /// \name  ROS parameters
    ///@{
    float camera_freq_;  ///<  Frequency of the sensor
    std::string fixed_frame_;   ///<  Frame in which the pose is expressed
    std::string camera_frame_;  ///<  Frame of the camera
    std::vector<float> fov_color_;  ///<  Color of the camera field of view Rviz marker
    float focal_length_;    ///<  Focal length of the camera
    float sensor_width_;    ///<  Width of the camera sensor
    float sensor_height_;   ///<  Height of the camera sensor
    float fov_distance_;    ///<  Maximum distance detected by the camera
    int n_pxl_height_;      ///<  Nbr of pixels along sensor height
    int n_pxl_width_;       ///<  Nbr of pixels along sensor width
    ///@}


    /**
     * \brief  Main callback which is called by a timer
     *
     * \param timer_event  Timer event information
     */
    void main_cb(const ros::TimerEvent &timer_event);

    /**
     * \brief  SINGINT (Ctrl+C) callback to stop the nodelet properly
     */
    static void sigint_handler(int s);

    /**
     * \brief  Callback for the algae of the farm
     *
     * \param msg  Pointer to the algae
     */
    void algae_cb(const mf_farm_simulator::AlgaeConstPtr msg);

    /**
     * \brief  Initialise the collision world
     */
    void init_coll_world();

    /**
     * \brief  Updates algae in the collision world
     */
    void update_algae();

    /**
     * \brief  Gets tf transform from fixed frame to camera
     *
     * \return  Whether a transform has been received
     */
    bool get_camera_tf();

    /**
     * \brief  Publishes a Rviz marker for the camera field of view
     */
    void publish_rviz_fov();

    /**
     * \brief  Selects algae that are in field of view of the camera
     */
    void overlap_fov();

    /**
     * \brief  Gets position, dimension and axes of the algae for raycasting
     *
     * \param [out] w_algae   Width of the algae
     * \param [out] h_algae   Height of the algae
     * \param [out] inc_y3    Increment along y3 algae axis
     * \param [out] inc_z3    Increment along z3 algae axis
     * \param [out] tf_algae  Transforms of algae local frames
     */
    void get_ray_algae_carac(
      std::vector<float> &w_algae, std::vector<float> &h_algae,
      std::vector<float> &inc_y3,  std::vector<float> &inc_z3,
      std::vector<geometry_msgs::TransformStamped> &tf_algae
    );


    /**
     * \brief  Casts a ray to get alga disease value at hit point
     *
     * \param [in]  aim_pt    Point towards which casting the ray (in camera frame)
     * \param [out] hit_pt    Hit point
     * \param [out] alga_idx  Index of the hit alga in the ray_bodies_ vector
     * \return  Whether an alga has been hit
     */
    bool raycast_alga(const tf2::Vector3 &aim_pt, tf2::Vector3 &hit_pt,
      int &alga_idx);

    /**
     * \brief  Prepares the ROS output messages
     *
     * \param [out] out_msgs    Camera output message
     * \param [out] ray_marker  Rviz marker for displaying the rays
     * \param [out] pts_marker  Rviz marker for displaying the hit points
     */
    void prepare_out_msgs(
      mf_sensors_simulator::CameraOutput &out_msg,
      visualization_msgs::Marker &ray_marker,
      visualization_msgs::Marker &pts_marker
    );


    /**
     * \brief  Add a point to a point marker
     *
     * \param [out] marker   Marker to fill
     * \param [in]  pt       Point to add to the marker
     * \param [in]  color_r  Red channel of the point color
     * \param [in]  color_g  Green channel of the point color
     * \param [in]  color_b  Blue channel of the point color
     */
    void add_pt_to_marker(visualization_msgs::Marker &marker,
      const tf2::Vector3 &pt, float color_r, float color_g, float color_b);

    /**
     * \brief  Add a point to a point marker
     *
     * \param [out] marker  Marker to fill
     * \param [in]  pt1     First point to add to the marker
     * \param [in]  pt2     Second point to add to the marker
     */
    void add_line_to_marker(visualization_msgs::Marker &marker,
      const tf2::Vector3 &pt1, const tf2::Vector3 &pt2);

    /**
     * \brief  Publishes camera output
     */
    void publish_output();

};


}  // namespace mfcpp

#endif