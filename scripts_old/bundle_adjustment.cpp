#include <cstdlib>
#include <ctime>

#include "bundle_adjustment.hpp"
#include "Eigen/Core"

using namespace std;
using namespace cv;
using namespace Eigen;


/* Solverの初期化
   - カメラの平行移動なし
   - カメラの回転移動なし
   - 各特徴点は (0, 0, 1000) ( 画素が2mm四方として1m先くらい )
 */
void bundleAdjustment::Solver::initialize() {
  cout << "initialzie" << endl;

  std::srand(std::time(0));

  double cam_scale = 0.2;

  for( int i = 0; i < Nc; ++i ) {
    if ( i == 0 ) {
      cam_t_x[i] = 0;
      cam_t_y[i] = 0;
      cam_t_z[i] = 0;
    } else if ( i == 1 ) {
      cam_t_x[i] = cam_scale * ( (double)std::rand() / RAND_MAX - 0.5 );
      cam_t_y[i] = cam_scale;
      cam_t_z[i] = cam_scale * ( (double)std::rand() / RAND_MAX - 0.5 );
    } else {
      cam_t_x[i] = cam_scale * ( (double)std::rand() / RAND_MAX - 0.5 );
      cam_t_y[i] = cam_scale * ( (double)std::rand() / RAND_MAX - 0.5 );
      cam_t_z[i] = cam_scale * ( (double)std::rand() / RAND_MAX - 0.5 );
    }

    cam_pose_x[i] = 0; cam_pose_y[i] = 0; cam_pose_z[i] = 0;
  }
  
  double f = 1.0;

  for( int j = 0; j < Np; ++j ) {
    double depth = 1000.0 * ((2.0 / RAND_MAX) *(double)std::rand() + 2.0);
    point_x[j] = captured_x[0][j];
    point_y[j] = captured_y[0][j];
    point_z[j] = f / depth;
  }

  reprojection_error = ba_reprojection_error( *this );

}

void bundleAdjustment::Solver::mock_init( vector<Point3f> pt_vec, vector<Point3f> cam_t_vec, vector<Point3f> cam_rot_vec)
{
  cout << "mock_init" << endl;

  for( int i = 0; i < Nc; ++i ) {
    cam_t_x[i] = cam_t_vec[i].x;
    cam_t_y[i] = cam_t_vec[i].y;
    cam_t_z[i] = cam_t_vec[i].z;
    cam_pose_x[i] = cam_rot_vec[i].x;
    cam_pose_y[i] = cam_rot_vec[i].y;
    cam_pose_z[i] = cam_rot_vec[i].z;

    printf("cam: (%lf, %lf, %lf), (%lf, %lf, %lf)\n", cam_t_x[i], cam_t_y[i], cam_t_z[i], cam_pose_x[i], cam_pose_y[i], cam_pose_z[i]);

  }

  cout << "cam_t_done" << endl;

  for( int j = 0; j < Np; ++j ) {
    point_x[j] = pt_vec[j].x;
    point_y[j] = pt_vec[j].y;
    point_z[j] = pt_vec[j].z;
  }

  cout << "cam_rot_done" << endl;
  reprojection_error = 1000;

  for( int i = 0; i < Nc; ++i ) {
    for( int j = 0; j < Np; ++j ) {
      reproject_x[i][j] = ba_reproject_x( *this, i, j);
      reproject_y[i][j] = ba_reproject_y( *this, i, j);
      reproject_z[i][j] = ba_reproject_z( *this, i, j);
    }
  }

  cout << "reproj = " << ba_reprojection_error( *this ) << endl;
}


/* Solverの計算を1ステップ進める。１ステップは以下の手順
   - 再投影点の計算
   - 再投影点の微分の計算
 */
void bundleAdjustment::Solver::run_one_step() {

  // 再投影点の計算
  for( int i = 0; i < Nc; ++i ) {
    for( int j = 0; j < Np; ++j ) {
      reproject_x[i][j] = ba_reproject_x( *this, i, j);
      reproject_y[i][j] = ba_reproject_y( *this, i, j);
      reproject_z[i][j] = ba_reproject_z( *this, i, j);
    }
  }

  cout << "reproject" << endl;

  // 再投影点の微分の計算
  for( int i = 0; i < Nc; ++i ) {
    for( int j = 0; j < Np; ++j ) {
      for( int k = 0; k < K; ++k ) {
	grad_reproject_x[i][j][k] = ba_get_reproject_gradient_x( *this, i, j, k);
	grad_reproject_y[i][j][k] = ba_get_reproject_gradient_y( *this, i, j, k);
	grad_reproject_z[i][j][k] = ba_get_reproject_gradient_z( *this, i, j, k);
      }
    }
  }
  
  cout << "gradient" << endl;

  Eigen::VectorXd v = ba_get_update_for_step( *this);
  cout << "update norm = " << v.norm() << endl;

  // 各変数を更新
  for( int i = 0; i < Nc; ++i ) { 
    cam_t_x[i] += v[i];
    cam_t_y[i] += v[ i+Nc ];
    cam_t_z[i] += v[ i+2*Nc ];
    cam_pose_x[i] += v[ i+3*Nc ];
    cam_pose_y[i] += v[ i+4*Nc ];
    cam_pose_z[i] += v[ i+5*Nc ];
  }

  for( int j = 0; j < Np; ++j ) {
    point_x[j] += v[ j + 0*Np + 6*Nc ];
    point_y[j] += v[ j + 1*Np + 6*Nc ];
    point_z[j] += v[ j + 2*Np + 6*Nc ];
  }

  double prev_reprojection_error = this->reprojection_error;
  this->reprojection_error = ba_reprojection_error( *this );
  
  if( prev_reprojection_error - this->reprojection_error > 0 ) {
    this->c /= 10.0;
  } else {
    this->c *= 10.0;
  }

}


cv::Mat bundleAdjustment::Solver::depth_drawn( cv::Mat img, 
					       vector<Point2f> points, 
					       vector<double> depth)
{
  Mat dst = img.clone();
  for( int i = 0; i < points.size(); ++i ) {
    cv::Point2f pt = points[i];
    double s = 0.0002;
    double z = 1.0 / fabs(depth[i]);

    cv::Scalar color = cv::Scalar::all(255);
    cv::circle(img, pt, 2, color, 1, 8, 0);

    char buf[256];
    sprintf(buf, "%2.3lf", z*s);
    std::string str = buf;
    cv::putText(dst, str, pt, cv::FONT_HERSHEY_COMPLEX_SMALL, 0.6, color, 1, 1, false);

  }
  return dst;
}