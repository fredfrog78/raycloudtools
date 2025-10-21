// Copyright (c) 2020
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Thomas Lowe

#include "raylib/rayalignment.h"
#include "raylib/rayaxisalign.h"
#include "raylib/raycloud.h"
#include "raylib/rayfinealignment.h"
#include "raylib/rayparse.h"
#include "raylib/raypose.h"

#include <nabo/nabo.h>
#include <Eigen/Geometry>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <complex>
#include <iostream>

void usage(int exit_code = 1)
{
  // clang-format off
  std::cout << "Align raycloudA onto raycloudB, rigidly. Outputs the transformed version of raycloudA." << std::endl;
  std::cout << "This method is for when there is more than approximately 30% overlap between clouds." << std::endl;
  std::cout << "usage:" << std::endl;
  std::cout << "rayalign raycloudA raycloudB" << std::endl;
  std::cout << "                             --nonrigid - nonrigid (quadratic) alignment" << std::endl;
  std::cout << "                             --verbose  - outputs FFT images and the coarse alignment cloud" << std::endl;
  std::cout << "                             --local    - fine alignment only, assumes clouds are already approximately aligned" << std::endl;
  std::cout << "rayalign raycloud  - axis aligns to the walls, placing the major walls at (0,0,0), biggest along y." << std::endl;
  // clang-format on
  exit(exit_code);
}

int rayAlign(int argc, char *argv[])
{
  ray::FileArgument cloud_a, cloud_b;
  ray::OptionalFlagArgument nonrigid("nonrigid", 'n'), is_verbose("verbose", 'v'), local("local", 'l');
  bool cross_align = ray::parseCommandLine(argc, argv, { &cloud_a, &cloud_b }, { &nonrigid, &is_verbose, &local });
  bool self_align = ray::parseCommandLine(argc, argv, { &cloud_a });
  if (!cross_align && !self_align)
    usage();

  std::string aligned_name = cloud_a.nameStub() + "_aligned.ply";
  if (self_align)
  {
    if (!ray::alignCloudToAxes(cloud_a.name(), aligned_name))
      usage();
  }
  else  // cross_align
  {
    ray::Pose transform;
    ray::Cloud clouds[2];
    if (!clouds[0].load(cloud_a.name()))
      usage();
    if (!clouds[1].load(cloud_b.name()))
      usage();

    // Here we pick three distant points in the cloud as an independent method of determining the total transformation
    // applied
    size_t min_i = 0, max_i = 0, min_j = 0;
    for (size_t i = 0; i < clouds[0].ends.size(); i++)
    {
      if (clouds[0].ends[i][0] < clouds[0].ends[min_i][0])
        min_i = i;
      if (clouds[0].ends[i][0] > clouds[0].ends[max_i][0])
        max_i = i;
      if (clouds[0].ends[i][1] < clouds[0].ends[min_j][1])
        min_j = i;
    }
    Eigen::Matrix3Xd p_orig(3, 3);
    p_orig.col(0) = clouds[0].ends[min_i];
    p_orig.col(1) = clouds[0].ends[max_i];
    p_orig.col(2) = clouds[0].ends[min_j];

    bool local_only = local.isSet();
    bool non_rigid = nonrigid.isSet();
    bool verbose = is_verbose.isSet();
    if (!local_only)
    {
      alignCloud0ToCloud1(clouds, 0.5, verbose);
      if (verbose)
        clouds[0].save(cloud_a.nameStub() + "_coarse_aligned.ply");
    }

    ray::FineAlignment fineAlign(clouds, non_rigid, verbose);
    fineAlign.align();

    // Now we calculate the rigid transformation from the change in the position of the three points:
    Eigen::Matrix3Xd p_aligned(3, 3);
    p_aligned.col(0) = clouds[0].ends[min_i];
    p_aligned.col(1) = clouds[0].ends[max_i];
    p_aligned.col(2) = clouds[0].ends[min_j];

    Eigen::Matrix4d transform_matrix = Eigen::umeyama(p_orig, p_aligned, false);
    Eigen::Matrix3d rotation = transform_matrix.block<3,3>(0,0);
    Eigen::Vector3d translation = transform_matrix.block<3,1>(0,3);

    Eigen::Vector3d euler_angles = rotation.eulerAngles(0, 1, 2); // roll, pitch, yaw
    std::cout << "Transformation of " << cloud_a.nameStub() << ":" << std::endl;
    std::cout << "          rotation: (" << euler_angles[0] * 180.0 / ray::kPi << ", "
              << euler_angles[1] * 180.0 / ray::kPi << ", "
              << euler_angles[2] * 180.0 / ray::kPi << ") degrees " << std::endl;
    std::cout << "  then translation: (" << translation.transpose() << ")" << std::endl;
    if (non_rigid)
    {
      std::cout << "This rigid transformation is approximate as a non-rigid transformation was applied" << std::endl;
    }

    clouds[0].save(aligned_name);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  return ray::runWithMemoryCheck(rayAlign, argc, argv);
}
