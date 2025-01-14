/*
README:
Change the following flags to process your own video:
- video // Path to your video to process
- video_output // Path to the output location
- aspect_ratio_rotate // Write here 0 for landscape videos and 90 for portrait videos

Our code is based on the example 3_user_synchronous_output.
You find our extended code for the classifier and video export on line 245-406 and 572-581.
*/



// C++ std library dependencies
#include <chrono> // `std::chrono::` functions and classes, e.g. std::chrono::milliseconds
#include <thread> // std::this_thread
// Other 3rdparty dependencies
// GFlags: DEFINE_bool, _int32, _int64, _uint64, _double, _string
#include <gflags/gflags.h>
// Allow Google Flags in Ubuntu 14
#ifndef GFLAGS_GFLAGS_H_
    namespace gflags = google;
#endif
// OpenPose dependencies
#include <openpose/headers.hpp>

#include <iostream>
#include <windows.h>


DEFINE_int32(logging_level,             3,              "The logging level. Integer in the range [0, 255]. 0 will output any log() message, while"
                                                        " 255 will not output any. Current OpenPose library messages are in the range 0-4: 1 for"
                                                        " low priority messages and 4 for important ones.");
DEFINE_bool(disable_multi_thread,       false,          "It would slightly reduce the frame rate in order to highly reduce the lag. Mainly useful"
                                                        " for 1) Cases where it is needed a low latency (e.g. webcam in real-time scenarios with"
                                                        " low-range GPU devices); and 2) Debugging OpenPose when it is crashing to locate the"
                                                        " error.");
DEFINE_int32(profile_speed,             1000,           "If PROFILER_ENABLED was set in CMake or Makefile.config files, OpenPose will show some"
                                                        " runtime statistics at this frame number.");
// Producer
DEFINE_int32(camera,                    -1,             "The camera index for cv::VideoCapture. Integer in the range [0, 9]. Select a negative"
                                                        " number (by default), to auto-detect and open the first available camera.");
DEFINE_string(camera_resolution,        "-1x-1",        "Set the camera resolution (either `--camera` or `--flir_camera`). `-1x-1` will use the"
                                                        " default 1280x720 for `--camera`, or the maximum flir camera resolution available for"
                                                        " `--flir_camera`");
DEFINE_double(camera_fps,               30.0,           "Frame rate for the webcam (also used when saving video). Set this value to the minimum"
                                                        " value between the OpenPose displayed speed and the webcam real frame rate.");
DEFINE_string(video,                    "examples/media/resized/cutted/IMG_5852.mp4",             "Use a video file instead of the camera. Use `examples/media/video.avi` for our default  "
                                                        " example video.");
DEFINE_string(video_output, "examples/media/export/IMG_5852.mp4", "");
DEFINE_int32(aspect_ratio_rotate, 90, "Rotate each frame, 4 possible values: 0, 90, 180, 270.");
DEFINE_int32(frame_rotate, 0, "Rotate each frame, 4 possible values: 0, 90, 180, 270.");
DEFINE_string(image_dir,                "",             "Process a directory of images. Use `examples/media/` for our default example folder with 20"
                                                        " images. Read all standard formats (jpg, png, bmp, etc.).");
DEFINE_bool(flir_camera,                false,          "Whether to use FLIR (Point-Grey) stereo camera.");
DEFINE_string(ip_camera,                "",             "String with the IP camera URL. It supports protocols like RTSP and HTTP.");
DEFINE_uint64(frame_first,              0,              "Start on desired frame number. Indexes are 0-based, i.e. the first frame has index 0.");
DEFINE_uint64(frame_last,               -1,             "Finish on desired frame number. Select -1 to disable. Indexes are 0-based, e.g. if set to"
                                                        " 10, it will process 11 frames (0-10).");
DEFINE_bool(frame_flip,                 false,          "Flip/mirror each frame (e.g. for real time webcam demonstrations).");
DEFINE_bool(frames_repeat,              false,          "Repeat frames when finished.");
DEFINE_bool(process_real_time,          false,          "Enable to keep the original source frame rate (e.g. for video). If the processing time is"
                                                        " too long, it will skip frames. If it is too fast, it will slow it down.");
DEFINE_string(camera_parameter_folder,  "models/cameraParameters/flir/", "String with the folder where the camera parameters are located.");
// OpenPose
DEFINE_string(model_folder,             "models/",      "Folder path (absolute or relative) where the models (pose, face, ...) are located.");
DEFINE_string(output_resolution,        "-1x-1",        "The image resolution (display and output). Use \"-1x-1\" to force the program to use the"
                                                        " input image resolution.");
DEFINE_int32(num_gpu,                   -1,             "The number of GPU devices to use. If negative, it will use all the available GPUs in your"
                                                        " machine.");
DEFINE_int32(num_gpu_start,             0,              "GPU device start number.");
DEFINE_int32(keypoint_scale,            0,              "Scaling of the (x,y) coordinates of the final pose data array, i.e. the scale of the (x,y)"
                                                        " coordinates that will be saved with the `write_keypoint` & `write_keypoint_json` flags."
                                                        " Select `0` to scale it to the original source resolution, `1`to scale it to the net output"
                                                        " size (set with `net_resolution`), `2` to scale it to the final output size (set with"
                                                        " `resolution`), `3` to scale it in the range [0,1], and 4 for range [-1,1]. Non related"
                                                        " with `scale_number` and `scale_gap`.");
DEFINE_int32(number_people_max,         -1,             "This parameter will limit the maximum number of people detected, by keeping the people with"
                                                        " top scores. The score is based in person area over the image, body part score, as well as"
                                                        " joint score (between each pair of connected body parts). Useful if you know the exact"
                                                        " number of people in the scene, so it can remove false positives (if all the people have"
                                                        " been detected. However, it might also include false negatives by removing very small or"
                                                        " highly occluded people. -1 will keep them all.");
// OpenPose Body Pose
DEFINE_bool(body_disable,               false,          "Disable body keypoint detection. Option only possible for faster (but less accurate) face"
                                                        " keypoint detection.");
DEFINE_string(model_pose,               "COCO",         "Model to be used. E.g. `COCO` (18 keypoints), `MPI` (15 keypoints, ~10% faster), "
                                                        "`MPI_4_layers` (15 keypoints, even faster but less accurate).");
DEFINE_string(net_resolution,           "-1x368",       "Multiples of 16. If it is increased, the accuracy potentially increases. If it is"
                                                        " decreased, the speed increases. For maximum speed-accuracy balance, it should keep the"
                                                        " closest aspect ratio possible to the images or videos to be processed. Using `-1` in"
                                                        " any of the dimensions, OP will choose the optimal aspect ratio depending on the user's"
                                                        " input value. E.g. the default `-1x368` is equivalent to `656x368` in 16:9 resolutions,"
                                                        " e.g. full HD (1980x1080) and HD (1280x720) resolutions.");
DEFINE_int32(scale_number,              1,              "Number of scales to average.");
DEFINE_double(scale_gap,                0.3,            "Scale gap between scales. No effect unless scale_number > 1. Initial scale is always 1."
                                                        " If you want to change the initial scale, you actually want to multiply the"
                                                        " `net_resolution` by your desired initial scale.");
// OpenPose Body Pose Heatmaps and Part Candidates
DEFINE_bool(heatmaps_add_parts,         false,          "If true, it will fill op::Datum::poseHeatMaps array with the body part heatmaps, and"
                                                        " analogously face & hand heatmaps to op::Datum::faceHeatMaps & op::Datum::handHeatMaps."
                                                        " If more than one `add_heatmaps_X` flag is enabled, it will place then in sequential"
                                                        " memory order: body parts + bkg + PAFs. It will follow the order on"
                                                        " POSE_BODY_PART_MAPPING in `src/openpose/pose/poseParameters.cpp`. Program speed will"
                                                        " considerably decrease. Not required for OpenPose, enable it only if you intend to"
                                                        " explicitly use this information later.");
DEFINE_bool(heatmaps_add_bkg,           false,          "Same functionality as `add_heatmaps_parts`, but adding the heatmap corresponding to"
                                                        " background.");
DEFINE_bool(heatmaps_add_PAFs,          false,          "Same functionality as `add_heatmaps_parts`, but adding the PAFs.");
DEFINE_int32(heatmaps_scale,            2,              "Set 0 to scale op::Datum::poseHeatMaps in the range [-1,1], 1 for [0,1]; 2 for integer"
                                                        " rounded [0,255]; and 3 for no scaling.");
DEFINE_bool(part_candidates,            false,          "Also enable `write_json` in order to save this information. If true, it will fill the"
                                                        " op::Datum::poseCandidates array with the body part candidates. Candidates refer to all"
                                                        " the detected body parts, before being assembled into people. Note that the number of"
                                                        " candidates is equal or higher than the number of final body parts (i.e. after being"
                                                        " assembled into people). The empty body parts are filled with 0s. Program speed will"
                                                        " slightly decrease. Not required for OpenPose, enable it only if you intend to explicitly"
                                                        " use this information.");
// OpenPose Face
DEFINE_bool(face,                       false,          "Enables face keypoint detection. It will share some parameters from the body pose, e.g."
                                                        " `model_folder`. Note that this will considerable slow down the performance and increse"
                                                        " the required GPU memory. In addition, the greater number of people on the image, the"
                                                        " slower OpenPose will be.");
DEFINE_string(face_net_resolution,      "368x368",      "Multiples of 16 and squared. Analogous to `net_resolution` but applied to the face keypoint"
                                                        " detector. 320x320 usually works fine while giving a substantial speed up when multiple"
                                                        " faces on the image.");
// OpenPose Hand
DEFINE_bool(hand,                       false,          "Enables hand keypoint detection. It will share some parameters from the body pose, e.g."
                                                        " `model_folder`. Analogously to `--face`, it will also slow down the performance, increase"
                                                        " the required GPU memory and its speed depends on the number of people.");
DEFINE_string(hand_net_resolution,      "368x368",      "Multiples of 16 and squared. Analogous to `net_resolution` but applied to the hand keypoint"
                                                        " detector.");
DEFINE_int32(hand_scale_number,         1,              "Analogous to `scale_number` but applied to the hand keypoint detector. Our best results"
                                                        " were found with `hand_scale_number` = 6 and `hand_scale_range` = 0.4.");
DEFINE_double(hand_scale_range,         0.4,            "Analogous purpose than `scale_gap` but applied to the hand keypoint detector. Total range"
                                                        " between smallest and biggest scale. The scales will be centered in ratio 1. E.g. if"
                                                        " scaleRange = 0.4 and scalesNumber = 2, then there will be 2 scales, 0.8 and 1.2.");
DEFINE_bool(hand_tracking,              false,          "Adding hand tracking might improve hand keypoints detection for webcam (if the frame rate"
                                                        " is high enough, i.e. >7 FPS per GPU) and video. This is not person ID tracking, it"
                                                        " simply looks for hands in positions at which hands were located in previous frames, but"
                                                        " it does not guarantee the same person ID among frames.");
// OpenPose 3-D Reconstruction
DEFINE_bool(3d,                         false,          "Running OpenPose 3-D reconstruction demo: 1) Reading from a stereo camera system."
                                                        " 2) Performing 3-D reconstruction from the multiple views. 3) Displaying 3-D reconstruction"
                                                        " results. Note that it will only display 1 person. If multiple people is present, it will"
                                                        " fail.");
DEFINE_int32(3d_min_views,              -1,             "Minimum number of views required to reconstruct each keypoint. By default (-1), it will"
                                                        " require all the cameras to see the keypoint in order to reconstruct it.");
DEFINE_int32(3d_views,                  1,              "Complementary option to `--image_dir` or `--video`. OpenPose will read as many images per"
                                                        " iteration, allowing tasks such as stereo camera processing (`--3d`). Note that"
                                                        " `--camera_parameters_folder` must be set. OpenPose must find as many `xml` files in the"
                                                        " parameter folder as this number indicates.");
// OpenPose identification
DEFINE_bool(identification,             false,          "Whether to enable people identification across frames. Not available yet, coming soon.");
// OpenPose Rendering
DEFINE_int32(part_to_show,              0,              "Prediction channel to visualize (default: 0). 0 for all the body parts, 1-18 for each body"
                                                        " part heat map, 19 for the background heat map, 20 for all the body part heat maps"
                                                        " together, 21 for all the PAFs, 22-40 for each body part pair PAF.");
DEFINE_bool(disable_blending,           false,          "If enabled, it will render the results (keypoint skeletons or heatmaps) on a black"
                                                        " background, instead of being rendered into the original image. Related: `part_to_show`,"
                                                        " `alpha_pose`, and `alpha_pose`.");
// OpenPose Rendering Pose
DEFINE_double(render_threshold,         0.05,           "Only estimated keypoints whose score confidences are higher than this threshold will be"
                                                        " rendered. Generally, a high threshold (> 0.5) will only render very clear body parts;"
                                                        " while small thresholds (~0.1) will also output guessed and occluded keypoints, but also"
                                                        " more false positives (i.e. wrong detections).");
DEFINE_int32(render_pose,               -1,             "Set to 0 for no rendering, 1 for CPU rendering (slightly faster), and 2 for GPU rendering"
                                                        " (slower but greater functionality, e.g. `alpha_X` flags). If -1, it will pick CPU if"
                                                        " CPU_ONLY is enabled, or GPU if CUDA is enabled. If rendering is enabled, it will render"
                                                        " both `outputData` and `cvOutputData` with the original image and desired body part to be"
                                                        " shown (i.e. keypoints, heat maps or PAFs).");
DEFINE_double(alpha_pose,               0.6,            "Blending factor (range 0-1) for the body part rendering. 1 will show it completely, 0 will"
                                                        " hide it. Only valid for GPU rendering.");
DEFINE_double(alpha_heatmap,            0.7,            "Blending factor (range 0-1) between heatmap and original frame. 1 will only show the"
                                                        " heatmap, 0 will only show the frame. Only valid for GPU rendering.");
// OpenPose Rendering Face
DEFINE_double(face_render_threshold,    0.4,            "Analogous to `render_threshold`, but applied to the face keypoints.");
DEFINE_int32(face_render,               -1,             "Analogous to `render_pose` but applied to the face. Extra option: -1 to use the same"
                                                        " configuration that `render_pose` is using.");
DEFINE_double(face_alpha_pose,          0.6,            "Analogous to `alpha_pose` but applied to face.");
DEFINE_double(face_alpha_heatmap,       0.7,            "Analogous to `alpha_heatmap` but applied to face.");
// OpenPose Rendering Hand
DEFINE_double(hand_render_threshold,    0.2,            "Analogous to `render_threshold`, but applied to the hand keypoints.");
DEFINE_int32(hand_render,               -1,             "Analogous to `render_pose` but applied to the hand. Extra option: -1 to use the same"
                                                        " configuration that `render_pose` is using.");
DEFINE_double(hand_alpha_pose,          0.6,            "Analogous to `alpha_pose` but applied to hand.");
DEFINE_double(hand_alpha_heatmap,       0.7,            "Analogous to `alpha_heatmap` but applied to hand.");
// Display
DEFINE_bool(fullscreen,                 false,          "Run in full-screen mode (press f during runtime to toggle).");
DEFINE_bool(no_gui_verbose,             false,          "Do not write text on output images on GUI (e.g. number of current frame and people). It"
                                                        " does not affect the pose rendering.");
DEFINE_int32(display,                   -1,             "Display mode: -1 for automatic selection; 0 for no display (useful if there is no X server"
                                                        " and/or to slightly speed up the processing if visual output is not required); 2 for 2-D"
                                                        " display; 3 for 3-D display (if `--3d` enabled); and 1 for both 2-D and 3-D display.");
// Result Saving
DEFINE_string(write_images,             "",             "Directory to write rendered frames in `write_images_format` image format.");
DEFINE_string(write_images_format,      "png",          "File extension and format for `write_images`, e.g. png, jpg or bmp. Check the OpenCV"
                                                        " function cv::imwrite for all compatible extensions.");
DEFINE_string(write_video,              "",             "Full file path to write rendered frames in motion JPEG video format. It might fail if the"
                                                        " final path does not finish in `.avi`. It internally uses cv::VideoWriter.");
DEFINE_string(write_json,               "",             "Directory to write OpenPose output in JSON format. It includes body, hand, and face pose"
                                                        " keypoints (2-D and 3-D), as well as pose candidates (if `--part_candidates` enabled).");
DEFINE_string(write_coco_json,          "",             "Full file path to write people pose data with JSON COCO validation format.");
DEFINE_string(write_heatmaps,           "",             "Directory to write body pose heatmaps in PNG format. At least 1 `add_heatmaps_X` flag"
                                                        " must be enabled.");
DEFINE_string(write_heatmaps_format,    "png",          "File extension and format for `write_heatmaps`, analogous to `write_images_format`."
                                                        " For lossless compression, recommended `png` for integer `heatmaps_scale` and `float` for"
                                                        " floating values.");
DEFINE_string(write_keypoint,           "",             "(Deprecated, use `write_json`) Directory to write the people pose keypoint data. Set format"
                                                        " with `write_keypoint_format`.");
DEFINE_string(write_keypoint_format,    "yml",          "(Deprecated, use `write_json`) File extension and format for `write_keypoint`: json, xml,"
                                                        " yaml & yml. Json not available for OpenCV < 3.0, use `write_keypoint_json` instead.");
DEFINE_string(write_keypoint_json,      "",             "(Deprecated, use `write_json`) Directory to write people pose data in JSON format,"
                                                        " compatible with any OpenCV version.");

using namespace std;
using namespace cv;

VideoWriter outputVideo;
string filename;

Size outputsize = FLAGS_aspect_ratio_rotate == 0 ? Size(640, 360) : Size(360, 640);

// If the user needs his own variables, he can inherit the op::Datum struct and add them
// UserDatum can be directly used by the OpenPose wrapper because it inherits from op::Datum, just define
// Wrapper<UserDatum> instead of Wrapper<op::Datum>
struct UserDatum : public op::Datum
{
    bool boolThatUserNeedsForSomeReason;

    UserDatum(const bool boolThatUserNeedsForSomeReason_ = false) :
        boolThatUserNeedsForSomeReason{boolThatUserNeedsForSomeReason_}
    {}
};

// The W-classes can be implemented either as a template or as simple classes given
// that the user usually knows which kind of data he will move between the queues,
// in this case we assume a std::shared_ptr of a std::vector of UserDatum

// This worker will just read and return all the jpg files in a directory
class WUserOutput : public op::WorkerConsumer<std::shared_ptr<std::vector<UserDatum>>>
{
public:

	const static int maxPersons = 2;
	bool warOben[maxPersons];
	int hampelmannCount[maxPersons];

    void initializationOnThread() {
		for (int i = 0; i < maxPersons; i++) {
			warOben[i] = false;
			hampelmannCount[i] = 0;
		}
	}


	string bodypartname[18] = {	"0 Nase",
								"1 Brust",
								"2 Rechte Schulter",
								"3 Rechter Ellbogen",
								"4 Rechtes Handgelenk",
								"5 Linke Schulter",
								"6 Linker Ellbogen",
								"7 Linkes Handgelenk",
								"8 Rechte Huefte",
								"9 Rechtes Knie",
								"10 Rechter Knoechel",
								"11 Linke Huefte",
								"12 Linkes Knie",
								"13 Linker Knoechel",
								"14 Rechtes Auge",
								"15 Linkes Auge",
								"16 Rechtes Ohr",
								"17 Linkes Ohr" 
	};

	float personmapping[maxPersons];
	int personmappingi[maxPersons];

    void workConsumer(const std::shared_ptr<std::vector<UserDatum>>& datumsPtr)
    {
        try
        {
            // User's displaying/saving/other processing here
                // datum.cvOutputData: rendered frame with pose or heatmaps
                // datum.poseKeypoints: Array<float> with the estimated pose
            if (datumsPtr != nullptr && !datumsPtr->empty())
            {


				const auto& poseKeypoints = datumsPtr->at(0).poseKeypoints;




				for (int i = 0; i < maxPersons; i++) {
					personmapping[i] = 100000.0;
					personmappingi[i] = 0;
				}

				float min = 0.0;
				for (int i = 0; i < maxPersons; i++) {
					for (int person = 0; person < maxPersons && person < poseKeypoints.getSize(0); person++) {
						Point2f brust(poseKeypoints[{person, 1, 0}], poseKeypoints[{person, 1, 1}]);
						if (brust.x > min && brust.x < personmapping[i]) {
							cout << "i:" << i << " person:" << person << " brust.x:" << brust.x << endl;
							personmapping[i] = brust.x;
							personmappingi[i] = person;
						}
					}
					cout << "personmappingi[" << i << "]:" << personmappingi[i] << endl;
					min = personmapping[i];
				}

				cout << "personmappingi" << endl;
				cout << personmappingi[0] << " " << personmappingi[1] << endl;



				// Display rendered output image
				cv::Mat image = datumsPtr->at(0).cvOutputData;

				for (int person = 0; person < poseKeypoints.getSize(0) && person < maxPersons; person++)
				{

					Point2f nase(poseKeypoints[{person, 0, 0}], poseKeypoints[{person, 0, 1}]);
					Point2f brust(poseKeypoints[{person, 1, 0}], poseKeypoints[{person, 1, 1}]);
					Point2f rechteSchulter(poseKeypoints[{person, 2, 0}], poseKeypoints[{person, 2, 1}]);
					Point2f rechterEllbogen(poseKeypoints[{person, 3, 0}], poseKeypoints[{person, 3, 1}]);
					Point2f rechtesHandgelenk(poseKeypoints[{person, 4, 0}], poseKeypoints[{person, 4, 1}]);
					Point2f linkeSchulter(poseKeypoints[{person, 5, 0}], poseKeypoints[{person, 5, 1}]);
					Point2f linkerEllbogen(poseKeypoints[{person, 6, 0}], poseKeypoints[{person, 6, 1}]);
					Point2f linkesHandgelenk(poseKeypoints[{person, 7, 0}], poseKeypoints[{person, 7, 1}]);
					Point2f rechteHuefte(poseKeypoints[{person, 8, 0}], poseKeypoints[{person, 8, 1}]);
					Point2f rechtesKnie(poseKeypoints[{person, 9, 0}], poseKeypoints[{person, 9, 1}]);
					Point2f rechterKnoechel(poseKeypoints[{person, 10, 0}], poseKeypoints[{person, 10, 1}]);
					Point2f linkeHuefte(poseKeypoints[{person, 11, 0}], poseKeypoints[{person, 11, 1}]);
					Point2f linkesKnie(poseKeypoints[{person, 12, 0}], poseKeypoints[{person, 12, 1}]);
					Point2f linkerKnoechel(poseKeypoints[{person, 13, 0}], poseKeypoints[{person, 13, 1}]);
					Point2f rechtesAuge(poseKeypoints[{person, 14, 0}], poseKeypoints[{person, 14, 1}]);
					Point2f linkesAuge(poseKeypoints[{person, 15, 0}], poseKeypoints[{person, 15, 1}]);
					Point2f rechtesOhr(poseKeypoints[{person, 16, 0}], poseKeypoints[{person, 16, 1}]);
					Point2f linkesOhr(poseKeypoints[{person, 17, 0}], poseKeypoints[{person, 17, 1}]);

					// Position unten
					if (
						abs(linkesHandgelenk.x - rechtesHandgelenk.x) <= 2 * abs(linkeSchulter.x - rechteSchulter.x) &&
						linkesHandgelenk.y > brust.y &&
						rechtesHandgelenk.y > brust.y &&
						abs(linkerKnoechel.x - rechterKnoechel.x) <= 1 * abs(linkeSchulter.x - rechteSchulter.x)
						) {
						//op::log("###########  UNTEN  ##########");
						if (warOben[personmappingi[person]]) {
							hampelmannCount[personmappingi[person]]++;
							warOben[personmappingi[person]] = false;
							op::log("Anzahl Hampelmann: " + to_string(hampelmannCount[personmappingi[person]]));
						}
					}

					// Position oben
					if (
						abs(linkesHandgelenk.x - rechtesHandgelenk.x) <= 2 * abs(linkeSchulter.x - rechteSchulter.x) &&
						linkesHandgelenk.y < nase.y &&
						rechtesHandgelenk.y < nase.y &&
						abs(linkerKnoechel.x - rechterKnoechel.x) > 1 * abs(linkeSchulter.x - rechteSchulter.x)
						) {
						//op::log("###########  OBEN  ##########");
						warOben[personmappingi[person]] = true;
					}

					string text = to_string(hampelmannCount[personmappingi[person]]);
					int fontFace = FONT_HERSHEY_SIMPLEX;
					double fontScale = 2;
					int thickness = 3;
					Point textOrg(10, 60 + personmappingi[person] * 110);
					//Point textOrg(10 + person * 560 - (person > 0 && hampelmannCount[person] >= 10 ? 60 : 0), 80);
					//Point textOrg(10, 260);
					putText(image, text, textOrg, fontFace, fontScale, Scalar::all(255), thickness, 8);


					text = "P" + to_string(personmappingi[person] + 1);
					fontScale = 1;
					thickness = 3;
					Point textPersonCounterOrg(10, 95 + personmappingi[person] * 110);
					//Point textPersonCounterOrg(20 + person * 560 - (person > 0 && hampelmannCount[person] >= 10 ? 60 : 0), 120);

					putText(image, text, textPersonCounterOrg, fontFace, fontScale, Scalar::all(255), thickness, 8);


					text = "P"+to_string(personmappingi[person] +1);
					fontScale = 1;
					thickness = 3;
					Point textPersonOrg(brust.x-20, brust.y+30);

					putText(image, text, textPersonOrg, fontFace, fontScale, Scalar::all(255), thickness, 8);
				}

				cv::Mat imageResized;
				resize(image, imageResized, outputsize);


				outputVideo.write(imageResized);

                cv::imshow("User worker GUI", imageResized);
                // Display image and sleeps at least 1 ms (it usually sleeps ~5-10 msec to display the image)
                const char key = (char)cv::waitKey(1);
				if (key == 27)
					this->stop();


            }
        }
        catch (const std::exception& e)
        {
            this->stop();
            op::error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }
};

int openPoseDemo()
{

    // logging_level
    op::check(0 <= FLAGS_logging_level && FLAGS_logging_level <= 255, "Wrong logging_level value.",
              __LINE__, __FUNCTION__, __FILE__);
    op::ConfigureLog::setPriorityThreshold((op::Priority)FLAGS_logging_level);
    op::Profiler::setDefaultX(FLAGS_profile_speed);
    // // For debugging
    // // Print all logging messages
    // op::ConfigureLog::setPriorityThreshold(op::Priority::None);
    // // Print out speed values faster
    // op::Profiler::setDefaultX(100);

    op::log("Starting pose estimation demo.", op::Priority::High);
    const auto timerBegin = std::chrono::high_resolution_clock::now();

    // Applying user defined configuration - Google flags to program variables
    // outputSize
    const auto outputSize = op::flagsToPoint(FLAGS_output_resolution, "-1x-1");
    // netInputSize
    const auto netInputSize = op::flagsToPoint(FLAGS_net_resolution, "-1x368");
    // faceNetInputSize
    const auto faceNetInputSize = op::flagsToPoint(FLAGS_face_net_resolution, "368x368 (multiples of 16)");
    // handNetInputSize
    const auto handNetInputSize = op::flagsToPoint(FLAGS_hand_net_resolution, "368x368 (multiples of 16)");
    // producerType
    const auto producerSharedPtr = op::flagsToProducer(FLAGS_image_dir, FLAGS_video, FLAGS_ip_camera, FLAGS_camera,
                                                       FLAGS_flir_camera, FLAGS_camera_resolution, FLAGS_camera_fps,
                                                       FLAGS_camera_parameter_folder,
                                                       (unsigned int) FLAGS_3d_views);
    // poseModel
    const auto poseModel = op::flagsToPoseModel(FLAGS_model_pose);
    // JSON saving
    const auto writeJson = (!FLAGS_write_json.empty() ? FLAGS_write_json : FLAGS_write_keypoint_json);
    if (!FLAGS_write_keypoint.empty() || !FLAGS_write_keypoint_json.empty())
        op::log("Flags `write_keypoint` and `write_keypoint_json` are deprecated and will eventually be removed."
                " Please, use `write_json` instead.", op::Priority::Max);
    // keypointScale
    const auto keypointScale = op::flagsToScaleMode(FLAGS_keypoint_scale);
    // heatmaps to add
    const auto heatMapTypes = op::flagsToHeatMaps(FLAGS_heatmaps_add_parts, FLAGS_heatmaps_add_bkg,
                                                  FLAGS_heatmaps_add_PAFs);
    const auto heatMapScale = op::flagsToHeatMapScaleMode(FLAGS_heatmaps_scale);
    // >1 camera view?
    const auto multipleView = (FLAGS_3d || FLAGS_3d_views > 1 || FLAGS_flir_camera);
    // Enabling Google Logging
    const bool enableGoogleLogging = true;
    // Logging
    op::log("", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);

    // OpenPose wrapper
    op::log("Configuring OpenPose wrapper.", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);
    // op::Wrapper<std::vector<op::Datum>> opWrapper;
    op::Wrapper<std::vector<UserDatum>> opWrapper;

    // Initializing the user custom classes
    // GUI (Display)
    auto wUserOutput = std::make_shared<WUserOutput>();
    // Add custom processing
    const auto workerOutputOnNewThread = true;
    opWrapper.setWorkerOutput(wUserOutput, workerOutputOnNewThread);

    // Pose configuration (use WrapperStructPose{} for default and recommended configuration)
    const op::WrapperStructPose wrapperStructPose{!FLAGS_body_disable, netInputSize, outputSize, keypointScale,
                                                  FLAGS_num_gpu, FLAGS_num_gpu_start, FLAGS_scale_number,
                                                  (float)FLAGS_scale_gap,
                                                  op::flagsToRenderMode(FLAGS_render_pose, multipleView),
                                                  poseModel, !FLAGS_disable_blending, (float)FLAGS_alpha_pose,
                                                  (float)FLAGS_alpha_heatmap, FLAGS_part_to_show, FLAGS_model_folder,
                                                  heatMapTypes, heatMapScale, FLAGS_part_candidates,
                                                  (float)FLAGS_render_threshold, FLAGS_number_people_max,
                                                  enableGoogleLogging, FLAGS_3d, FLAGS_3d_min_views,
                                                  FLAGS_identification};
    // Face configuration (use op::WrapperStructFace{} to disable it)
    const op::WrapperStructFace wrapperStructFace{FLAGS_face, faceNetInputSize,
                                                  op::flagsToRenderMode(FLAGS_face_render, multipleView, FLAGS_render_pose),
                                                  (float)FLAGS_face_alpha_pose, (float)FLAGS_face_alpha_heatmap,
                                                  (float)FLAGS_face_render_threshold};
    // Hand configuration (use op::WrapperStructHand{} to disable it)
    const op::WrapperStructHand wrapperStructHand{FLAGS_hand, handNetInputSize, FLAGS_hand_scale_number,
                                                  (float)FLAGS_hand_scale_range, FLAGS_hand_tracking,
                                                  op::flagsToRenderMode(FLAGS_hand_render, multipleView, FLAGS_render_pose),
                                                  (float)FLAGS_hand_alpha_pose, (float)FLAGS_hand_alpha_heatmap,
                                                  (float)FLAGS_hand_render_threshold};
    // Producer (use default to disable any input)
    const op::WrapperStructInput wrapperStructInput{producerSharedPtr, FLAGS_frame_first, FLAGS_frame_last,
                                                    FLAGS_process_real_time, FLAGS_frame_flip, FLAGS_frame_rotate,
                                                    FLAGS_frames_repeat};
    // Consumer (comment or use default argument to disable any output)
    // const op::WrapperStructOutput wrapperStructOutput{op::flagsToDisplayMode(FLAGS_display, FLAGS_3d),
    //                                                   !FLAGS_no_gui_verbose, FLAGS_fullscreen, FLAGS_write_keypoint,
    const auto displayMode = op::DisplayMode::NoDisplay;
    const bool guiVerbose = false;
    const bool fullScreen = false;
    const op::WrapperStructOutput wrapperStructOutput{displayMode, guiVerbose, fullScreen, FLAGS_write_keypoint,
                                                      op::stringToDataFormat(FLAGS_write_keypoint_format),
                                                      writeJson, FLAGS_write_coco_json,
                                                      FLAGS_write_images, FLAGS_write_images_format, FLAGS_write_video,
                                                      FLAGS_camera_fps, FLAGS_write_heatmaps,
                                                      FLAGS_write_heatmaps_format};
    // Configure wrapper
    opWrapper.configure(wrapperStructPose, wrapperStructFace, wrapperStructHand, wrapperStructInput,
                        wrapperStructOutput);
    // Set to single-thread running (to debug and/or reduce latency)
    if (FLAGS_disable_multi_thread)
        opWrapper.disableMultiThreading();

    // Start processing
    // Two different ways of running the program on multithread environment
    op::log("Starting thread(s)", op::Priority::High);
    // Option a) Recommended - Also using the main thread (this thread) for processing (it saves 1 thread)
    // Start, run & stop threads
    opWrapper.exec();  // It blocks this thread until all threads have finished

    // // Option b) Keeping this thread free in case you want to do something else meanwhile, e.g. profiling the GPU
    // memory
    // // VERY IMPORTANT NOTE: if OpenCV is compiled with Qt support, this option will not work. Qt needs the main
    // // thread to plot visual results, so the final GUI (which uses OpenCV) would return an exception similar to:
    // // `QMetaMethod::invoke: Unable to invoke methods with return values in queued connections`
    // // Start threads
    // opWrapper.start();
    // // Profile used GPU memory
    //     // 1: wait ~10sec so the memory has been totally loaded on GPU
    //     // 2: profile the GPU memory
    // const auto sleepTimeMs = 10;
    // for (auto i = 0 ; i < 10000/sleepTimeMs && opWrapper.isRunning() ; i++)
    //     std::this_thread::sleep_for(std::chrono::milliseconds{sleepTimeMs});
    // op::Profiler::profileGpuMemory(__LINE__, __FUNCTION__, __FILE__);
    // // Keep program alive while running threads
    // while (opWrapper.isRunning())
    //     std::this_thread::sleep_for(std::chrono::milliseconds{sleepTimeMs});
    // // Stop and join threads
    // op::log("Stopping thread(s)", op::Priority::High);
    // opWrapper.stop();

    // Measuring total time
    const auto now = std::chrono::high_resolution_clock::now();
    const auto totalTimeSec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(now-timerBegin).count()
                            * 1e-9;
    const auto message = "Real-time pose estimation demo successfully finished. Total time: "
                       + std::to_string(totalTimeSec) + " seconds.";
    op::log(message, op::Priority::High);

    return 0;
}

int main(int argc, char *argv[])
{


	int ex = CV_FOURCC('H', '2', '6', '4');// static_cast<int>(inputVideo.get(CV_CAP_PROP_FOURCC));
	double fps = 29.948;// inputVideo.get(CV_CAP_PROP_FPS); // framerate of the created video stream

	outputVideo.open(FLAGS_video_output, ex, fps, outputsize, true);
	if (!outputVideo.isOpened())
	{
		cout << "Could not open the output video for write: " << endl;
		cin >> ex;
		return -1;
	}


    // Parsing command line flags
    gflags::ParseCommandLineFlags(&argc, &argv, true);


	
	


    // Running openPoseDemo
    return openPoseDemo();
}
