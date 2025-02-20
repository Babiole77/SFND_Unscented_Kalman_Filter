#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;
  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  n_x_ = 5;
  n_aug_ = 7;
  // initial state vector
  x_ = VectorXd(n_x_);
  // initial covariance matrix
  P_ = MatrixXd(n_x_, n_x_);
  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3.0;
  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.0;
  
  /**These are provided by the sensor manufacturer. */
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;
  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;
  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;
  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;
  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  /**section for measurement noise values */
  
  /*** Initialization. */
  is_initialized_ = false;
  // predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_ + 1);
  // create vector for weights
  weights_ = VectorXd(2*n_aug_+1);
  // Sigma point spreading parameter
  lambda_ = 3 - n_aug_;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
  if(!is_initialized_)
  {
    if(meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_)
    {
        double rho = meas_package.raw_measurements_[0];     // radial distance
        double phi = meas_package.raw_measurements_[1];     // bearing angle
        double rho_dot = meas_package.raw_measurements_[2]; // radial velocity
        double px = rho*cos(phi);
        double py = rho*sin(phi);
        double v = sqrt(rho_dot*sin(phi)*rho_dot*sin(phi) + rho_dot*cos(phi)*rho_dot*cos(phi));
        x_ << px,
              py,
              v,
              0,
              0;
        P_ << 1, 0, 0, 0, 0,
              0, 1, 0, 0, 0,
              0, 0, 1, 0, 0,
              0, 0, 0, 1, 0,
              0, 0, 0, 0, 1;
        previousTimestamp = meas_package.timestamp_;
        is_initialized_= true;
    }
    else if((meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)) // Lidar measurements
    {
    // std::cout << "Laser first";
        double px = meas_package.raw_measurements_[0];
        double py = meas_package.raw_measurements_[1];
        x_ << px,
              py,
              0,
              0,
              0;
        P_ << std_laspx_*std_laspx_, 0, 0, 0, 0,
              0, std_laspy_*std_laspy_, 0, 0, 0,
              0, 0, 5, 0, 0,
              0, 0, 0, 1, 0,
              0, 0, 0, 0, 1;
        previousTimestamp = meas_package.timestamp_;
        is_initialized_= true;
    }
  // P_.fill(100);
  return;
  }
  weights_.fill(0.0);
  // set weights
  double weight_0 = lambda_/(lambda_ + n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i < 2*n_aug_+1; ++i) 
  {
    weights_(i) = 0.5/(n_aug_ + lambda_);
  }
  double dt = (meas_package.timestamp_ - previousTimestamp)/1000000.0;
  previousTimestamp = meas_package.timestamp_;
  Prediction(dt);
  if(use_laser_ && meas_package.sensor_type_ == meas_package.LASER)
    UpdateLidar(meas_package);
  else if(use_radar_ && meas_package.sensor_type_ == meas_package.RADAR)
    UpdateRadar(meas_package);
}

void UKF::Prediction(double delta_t) 
{
  /**
   * Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */
  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2*n_aug_ + 1);
  // augmented mean state
  x_aug.head(n_x_) = x_;
  x_aug(n_x_) = 0;
  x_aug(n_x_ + 1) = 0;
  //augmented covariance
  P_aug.fill(0);
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(n_x_, n_x_) = std_a_*std_a_;
  P_aug(n_x_+1, n_x_+1) = std_yawdd_*std_yawdd_;
  // Square Root Matrix
  MatrixXd L = P_aug.llt().matrixL();
  // Augmented sigma points
  Xsig_aug.fill(0);
  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i < n_aug_; ++i)
  {
    Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_)*L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_)*L.col(i);
  }
  // create matrix with predicted sigma points
  // predict sigma points
  for (int i = 0; i < 2*n_aug_+1; ++i) 
  {
    // extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    // predicted state values
    double px_p, py_p;

    // avoid division by zero
    if (fabs(yawd) > 0.001) 
    {
        px_p = p_x + v/yawd * (sin(yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * (cos(yaw) - cos(yaw+yawd*delta_t) );
    } 
    else 
    {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    // add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    // write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  // predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) 
  {  // iterate over sigma points
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }
  // std::cout << "Predicted state: " << x_ << std::endl;
  // predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) 
  {  
    // iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3) > M_PI) x_diff(3) -= 2.*M_PI;
    while (x_diff(3) < -M_PI) x_diff(3) += 2.*M_PI;
    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
  }
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * Lidar data is used to update the belief about the object's position.
   * Modifications: the state vector, x_, and covariance, P_.
   * NIS, if desired.
   */
  MatrixXd H = MatrixXd(2, n_x_);
  H <<  1, 0, 0, 0, 0,
        0, 1, 0, 0, 0;
  MatrixXd R = MatrixXd(2, 2); // lidar measurement covariance
  R <<  std_laspx_*std_laspx_, 0,
        0, std_laspy_*std_laspy_;
  VectorXd z = meas_package.raw_measurements_;
  VectorXd z_pred = H*x_;
  VectorXd y = z - z_pred;
  MatrixXd Ht = H.transpose();
  MatrixXd S = H*P_*Ht + R;
  MatrixXd Si = S.inverse();
  MatrixXd K = P_*Ht*Si;

  // new esimate
  x_ = x_ + K*y;
  MatrixXd I = MatrixXd::Identity(x_.size(), x_.size());
  P_ = (I - K*H)*P_; 

  float nis_lidar = ComputeNIS(z_pred, z, S);
  // std::cout << "NIS for Lidar: " << nis_lidar << std::endl;
  // static unsigned int over_95 = 0;
  // static unsigned int numMeas = 0;
  // ++numMeas;
  // if (nis_lidar > 6.0) ++over_95;
  // if (numMeas > 0) std::cout << "Over percentile lidar: " << 100.0*over_95/numMeas << std::endl;
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * Radar data is used to update the belief about the object's position. 
   * Modified: the state vector, x_, and covariance, P_.
   * NIS, if desired.
   */
  int n_z = 3; //measurement dimension: radar can measure rho, phi and rho_dot
  // sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2*n_aug_+ 1);
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  //measurement covariance matrix
  MatrixXd S = MatrixXd(n_z, n_z);

  //transform sigma points into measurement space
  for(int i = 0; i < 2*n_aug_ + 1; ++i)
  {
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = v*cos(yaw);
    double v2 = v*sin(yaw);

    //measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y); // rho
    Zsig(1,i) = atan2(p_y, p_x); // phi
    Zsig(2,i) = (p_x*v1 + p_y*v2)/sqrt(p_x*p_x + p_y*p_y); // ro_dot
  }

  //mean predicted measurement
  z_pred.fill(0);
  for(int i = 0; i < 2*n_aug_ + 1; ++i)
  {
    z_pred = z_pred + weights_(i)*Zsig.col(i);
  }

  // innovation covariance matrix S
  S.fill(0);
  for(int i = 0; i < 2*n_aug_ + 1; ++i)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while(z_diff(1) > M_PI) z_diff(1) -= 2.*M_PI;
    while(z_diff(1) < M_PI) z_diff(1) += 2.*M_PI;

    S = S + weights_(i)*z_diff*z_diff.transpose();
  }
  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z, n_z);
  R <<  std_radr_*std_radr_, 0, 0,
        0, std_radphi_*std_radphi_, 0,
        0, 0, std_radrd_*std_radrd_;
  S = S + R;

  // Matrix for cross-correlation
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  // calculate cross correlation
  Tc.fill(0);
  for(int i=0; i < 2*n_aug_ + 1; ++i)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while(z_diff(1) > M_PI) z_diff(1) -= 2.*M_PI;
    while(z_diff(1) < M_PI) z_diff(1) += 2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3) > M_PI) x_diff(3) -= 2.*M_PI;
    while (x_diff(3) < -M_PI) x_diff(3) += 2.*M_PI;
    Tc = Tc + weights_(i)*x_diff*z_diff.transpose();
  }
  // Kalman gain K
  MatrixXd K =  Tc * S.inverse();
  // residual
  VectorXd z = meas_package.raw_measurements_;
  VectorXd z_diff = z - z_pred;
  while(z_diff(1) > M_PI) z_diff(1) -= 2.*M_PI;
  while(z_diff(1) < M_PI) z_diff(1) += 2.*M_PI;
  // update state mean and covariance matrix
  x_ = x_ + K*z_diff;
  P_ = P_ - K*S*K.transpose();

  float nis_radar = ComputeNIS(z_pred, z, S);
  // std::cout << "NIS for radar " << nis_radar << std::endl;
  // static unsigned int over_95 = 0;
  // static unsigned int numMeas = 0;
  // ++numMeas;
  // if (nis_radar > 7.8) ++over_95;
  // if (numMeas > 0) std::cout << "Over percentile radar: " << 100.0*over_95/numMeas << std::endl;
}

float UKF::ComputeNIS(VectorXd z_pred, VectorXd z_meas, MatrixXd S)
{
  VectorXd z_diff = z_meas - z_pred;
  float nis = z_diff.transpose()*S.inverse()*z_diff;

  return nis;
}