#include "depth_from_sequence.hpp"

void dump_camera(Camera camera){
    cout << camera.t << " " << camera.rot << endl;
}


void print_params(BundleAdjustment::Solver &s) {
    cout << "BundleAdjustment::Solver estimated"  << endl;

    cout << "points"  << endl;
    for(int j = 0; j < s.Np; ++j )
        cout << s.points[j] << endl;

    cout << endl << "camera params : [trans], [rot]" << endl;
    for(int i = 0; i < s.Nc; ++i )
        cout << s.camera_params[i].t << " " << s.camera_params[i].rot << endl;

    cout << endl;
}

void print_ittr_status(BundleAdjustment::Solver &s) {
    cout << "BundleAdjustment::Solver itteration status" << endl;
    cout << "ittr = " << s.ittr << endl;
    cout << "reprojection error = " << s.reprojection_error() << endl;
    cout << "update step size = " << s.c << endl;
    cout << "update norm = " << s.update_norm << endl;

    cout << endl;
}

Mat1b warped_image(vector<Mat1b> images, vector<Camera> cameras, double depth) {
    int H = images[0].rows, W = images[0].cols;
    Mat1b ret(H,W);
    Camera ref_cam = cameras[0];

    vector<Matx33d> homos(cameras.size());
    for( int i = 0; i < homos.size(); ++i )
        homos[i] = PlaneSweep::homography_matrix(ref_cam, cameras[i], depth);


    for( int h = 0; h < H; ++h ) {
        for( int w = 0; w < W; ++w ) {
            double v = 0.0;
            for( int i = 1; i < homos.size(); ++i ) {
                Point2d pt = ps_homogenious_point( homos[i], Point2d(w, h));
                //Point2d pt = ps_homogenious_point_cam(cameras[i], Point2d(w, h), depth);
                if( pt.x < 0 || pt.x >= W || pt.y < 0 || pt.y >= H ) {
                    // do notihing
                } else {
                    v += images[i].at<uchar>((int)pt.y, (int)pt.x);
                }
            }
            ret.at<uchar>(h,w) = v/(double)homos.size();
        }
    }

    return ret;
}
