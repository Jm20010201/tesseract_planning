/**
 * @file default_problem_generator.h
 * @brief Generates a trajopt problem from a planner request
 *
 * @author Levi Armstrong
 * @date April 18, 2018
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2020, Southwest Research Institute
 *
 * @par License
 * Software License Agreement (Apache License)
 * @par
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * @par
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef TESSERACT_MOTION_PLANNERS_TRAJOPT_DEFAULT_PROBLEM_GENERATOR_H
#define TESSERACT_MOTION_PLANNERS_TRAJOPT_DEFAULT_PROBLEM_GENERATOR_H

#include <trajopt/problem_description.hpp>
#include <tesseract_command_language/command_language.h>
#include <tesseract_motion_planners/core/types.h>
#include <tesseract_motion_planners/trajopt/profile/trajopt_default_composite_profile.h>
#include <tesseract_motion_planners/trajopt/profile/trajopt_default_plan_profile.h>

namespace tesseract_planning
{
inline trajopt::TrajOptProb::Ptr DefaultTrajoptProblemGenerator(const PlannerRequest& request,
                                                                const TrajOptPlanProfileMap& plan_profiles,
                                                                const TrajOptCompositeProfileMap& composite_profiles)
{
  auto pci = std::make_shared<trajopt::ProblemConstructionInfo>(request.tesseract);

  // Store fixed steps
  std::vector<int> fixed_steps;

  // Assign Kinematics object
  pci->kin = pci->getManipulator(request.manipulator);
  std::string link = pci->kin->getTipLinkName();

  if (pci->kin == nullptr)
  {
    std::string error_msg = "In trajopt_array_planner: manipulator_ does not exist in kin_map_";
    CONSOLE_BRIDGE_logError(error_msg.c_str());
    throw std::runtime_error(error_msg);
  }

  // Check and make sure it does not contain any composite instruction
  for (const auto& instruction : request.instructions)
    if (isCompositeInstruction(instruction))
      throw std::runtime_error("Trajopt planner does not support child composite instructions.");

  // Get kinematics information
  tesseract_environment::Environment::ConstPtr env = request.tesseract->getEnvironmentConst();
  tesseract_environment::AdjacencyMap map(
      env->getSceneGraph(), pci->kin->getActiveLinkNames(), env->getCurrentState()->link_transforms);
  const std::vector<std::string>& active_links = map.getActiveLinkNames();

  // Create a temp seed storage.
  std::vector<Eigen::VectorXd> seed_states;
  seed_states.reserve(request.instructions.size());

  Waypoint start_waypoint = NullWaypoint();
  if (request.instructions.hasStartWaypoint())
  {
    start_waypoint = request.instructions.getStartWaypoint();
  }
  else
  {
    Eigen::VectorXd current_jv = env->getCurrentState()->getJointValues(pci->kin->getJointNames());
    JointWaypoint temp(current_jv);
    temp.joint_names = pci->kin->getJointNames();
    start_waypoint = temp;
  }

  // Transform plan instructions into trajopt cost and constraints
  int index = 0;
  bool found_plan_instruction{ false };
  for (std::size_t i = 0; i < request.instructions.size(); ++i)
  {
    const auto& instruction = request.instructions[i];
    if (isPlanInstruction(instruction))
    {
      assert(isPlanInstruction(instruction));
      const auto* plan_instruction = instruction.cast_const<PlanInstruction>();

      assert(isCompositeInstruction(request.seed[i]));
      const auto* seed_composite = request.seed[i].cast_const<tesseract_planning::CompositeInstruction>();
      auto interpolate_cnt = static_cast<int>(seed_composite->size());

      // Get Plan Profile
      std::string profile = plan_instruction->getProfile();
      if (profile.empty())
        profile = "DEFAULT";

      TrajOptPlanProfile::Ptr cur_plan_profile{ nullptr };
      auto it = plan_profiles.find(profile);
      if (it == plan_profiles.end())
        cur_plan_profile = std::make_shared<TrajOptDefaultPlanProfile>();
      else
        cur_plan_profile = it->second;

      std::size_t cartesian_seed_shift_index = 1;
      std::size_t freespace_seed_shift_index = 0;
      if (!found_plan_instruction)
      {
        // If this is the first plan instruction we reduce the interpolate cnt because the seed includes the start state
        --interpolate_cnt;

        // Add start seed state
        assert(isMoveInstruction(seed_composite->at(0)));
        const auto* seed_instruction = seed_composite->at(0).cast_const<tesseract_planning::MoveInstruction>();
        seed_states.push_back(seed_instruction->getPosition());

        cartesian_seed_shift_index = 0;
        freespace_seed_shift_index = 1;

        // Add start waypoint
        if (isCartesianWaypoint(start_waypoint))
        {
          const auto* cwp = start_waypoint.cast_const<Eigen::Isometry3d>();
          cur_plan_profile->apply(*pci, *cwp, *plan_instruction, active_links, index);
        }
        else if (isJointWaypoint(start_waypoint))
        {
          const auto* jwp = start_waypoint.cast_const<JointWaypoint>();
          cur_plan_profile->apply(*pci, *jwp, *plan_instruction, active_links, index);
        }
        else
        {
          throw std::runtime_error("DescartesMotionPlannerConfig: uknown waypoint type.");
        }

        ++index;
      }

      if (plan_instruction->isLinear())
      {
        if (isCartesianWaypoint(plan_instruction->getWaypoint()))
        {
          const auto* cur_wp = plan_instruction->getWaypoint().cast_const<tesseract_planning::CartesianWaypoint>();

          Eigen::Isometry3d prev_pose = Eigen::Isometry3d::Identity();
          if (isCartesianWaypoint(start_waypoint))
          {
            prev_pose = *(start_waypoint.cast_const<Eigen::Isometry3d>());
          }
          else if (isJointWaypoint(start_waypoint))
          {
            const auto* jwp = start_waypoint.cast_const<JointWaypoint>();
            if (!pci->kin->calcFwdKin(prev_pose, *jwp))
              throw std::runtime_error("TrajOptPlannerUniversalConfig: failed to solve forward kinematics!");

            prev_pose = pci->env->getCurrentState()->link_transforms.at(pci->kin->getBaseLinkName()) * prev_pose *
                        plan_instruction->getTCP();
          }
          else
          {
            throw std::runtime_error("TrajOptPlannerUniversalConfig: uknown waypoint type.");
          }

          tesseract_common::VectorIsometry3d poses = interpolate(prev_pose, *cur_wp, interpolate_cnt);
          // Add intermediate points with path costs and constraints
          for (std::size_t p = 1; p < poses.size() - 1; ++p)
          {
            cur_plan_profile->apply(*pci, poses[p], *plan_instruction, active_links, index);

            // Add seed state
            assert(isMoveInstruction(seed_composite->at(p - cartesian_seed_shift_index)));
            const auto* seed_instruction =
                seed_composite->at(p - cartesian_seed_shift_index).cast_const<tesseract_planning::MoveInstruction>();
            seed_states.push_back(seed_instruction->getPosition());

            ++index;
          }

          // Add final point with waypoint
          cur_plan_profile->apply(*pci, *cur_wp, *plan_instruction, active_links, index);

          // Add seed state
          assert(isMoveInstruction(seed_composite->back()));
          const auto* seed_instruction = seed_composite->back().cast_const<tesseract_planning::MoveInstruction>();
          seed_states.push_back(seed_instruction->getPosition());

          ++index;
        }
        else if (isJointWaypoint(plan_instruction->getWaypoint()))
        {
          const auto* cur_wp = plan_instruction->getWaypoint().cast_const<JointWaypoint>();
          Eigen::Isometry3d cur_pose = Eigen::Isometry3d::Identity();
          if (!pci->kin->calcFwdKin(cur_pose, *cur_wp))
            throw std::runtime_error("TrajOptPlannerUniversalConfig: failed to solve forward kinematics!");

          cur_pose = pci->env->getCurrentState()->link_transforms.at(pci->kin->getBaseLinkName()) * cur_pose *
                     plan_instruction->getTCP();

          Eigen::Isometry3d prev_pose = Eigen::Isometry3d::Identity();
          if (isCartesianWaypoint(start_waypoint))
          {
            prev_pose = *(start_waypoint.cast_const<Eigen::Isometry3d>());
          }
          else if (isJointWaypoint(start_waypoint))
          {
            const auto* jwp = start_waypoint.cast_const<JointWaypoint>();
            if (!pci->kin->calcFwdKin(prev_pose, *jwp))
              throw std::runtime_error("TrajOptPlannerUniversalConfig: failed to solve forward kinematics!");

            prev_pose = pci->env->getCurrentState()->link_transforms.at(pci->kin->getBaseLinkName()) * prev_pose *
                        plan_instruction->getTCP();
          }
          else
          {
            throw std::runtime_error("TrajOptPlannerUniversalConfig: uknown waypoint type.");
          }

          tesseract_common::VectorIsometry3d poses = interpolate(prev_pose, cur_pose, interpolate_cnt);
          // Add intermediate points with path costs and constraints
          for (std::size_t p = 1; p < poses.size() - 1; ++p)
          {
            cur_plan_profile->apply(*pci, poses[p], *plan_instruction, active_links, index);

            // Add seed state
            assert(isMoveInstruction(seed_composite->at(p - cartesian_seed_shift_index)));
            const auto* seed_instruction =
                seed_composite->at(p - cartesian_seed_shift_index).cast_const<tesseract_planning::MoveInstruction>();
            seed_states.push_back(seed_instruction->getPosition());

            ++index;
          }

          // Add final point with waypoint
          cur_plan_profile->apply(*pci, *cur_wp, *plan_instruction, active_links, index);

          // Add seed state
          assert(isMoveInstruction(seed_composite->back()));
          const auto* seed_instruction = seed_composite->back().cast_const<tesseract_planning::MoveInstruction>();
          seed_states.push_back(seed_instruction->getPosition());

          ++index;
        }
        else
        {
          throw std::runtime_error("TrajOptPlannerUniversalConfig: unknown waypoint type");
        }
      }
      else if (plan_instruction->isFreespace())
      {
        if (isJointWaypoint(plan_instruction->getWaypoint()))
        {
          const auto* cur_wp = plan_instruction->getWaypoint().cast_const<tesseract_planning::JointWaypoint>();

          // Add intermediate points with path costs and constraints
          for (std::size_t s = freespace_seed_shift_index; s < seed_composite->size() - 1; ++s)
          {
            // Add seed state
            assert(isMoveInstruction(seed_composite->at(s)));
            const auto* seed_instruction = seed_composite->at(s).cast_const<tesseract_planning::MoveInstruction>();
            seed_states.push_back(seed_instruction->getPosition());

            ++index;
          }

          // Add final point with waypoint costs and contraints
          /** @todo Should check that the joint names match the order of the manipulator */
          cur_plan_profile->apply(*pci, *cur_wp, *plan_instruction, active_links, index);

          // Add to fixed indices
          fixed_steps.push_back(index);

          // Add seed state
          assert(isMoveInstruction(seed_composite->back()));
          const auto* seed_instruction = seed_composite->back().cast_const<tesseract_planning::MoveInstruction>();
          seed_states.push_back(seed_instruction->getPosition());
        }
        else if (isCartesianWaypoint(plan_instruction->getWaypoint()))
        {
          const auto* cur_wp = plan_instruction->getWaypoint().cast_const<tesseract_planning::CartesianWaypoint>();

          // Add intermediate points with path costs and constraints
          for (std::size_t s = freespace_seed_shift_index; s < seed_composite->size() - 1; ++s)
          {
            // Add seed state
            assert(isMoveInstruction(seed_composite->at(s)));
            const auto* seed_instruction = seed_composite->at(s).cast_const<tesseract_planning::MoveInstruction>();
            seed_states.push_back(seed_instruction->getPosition());

            ++index;
          }

          // Add final point with waypoint costs and contraints
          /** @todo Should check that the joint names match the order of the manipulator */
          cur_plan_profile->apply(*pci, *cur_wp, *plan_instruction, active_links, index);

          // Add to fixed indices
          fixed_steps.push_back(index);

          // Add seed state
          assert(isMoveInstruction(seed_composite->back()));
          const auto* seed_instruction = seed_composite->back().cast_const<tesseract_planning::MoveInstruction>();
          seed_states.push_back(seed_instruction->getPosition());
        }
        else
        {
          throw std::runtime_error("TrajOptPlannerUniversalConfig: unknown waypoint type");
        }

        ++index;
      }
      else
      {
        throw std::runtime_error("Unsupported!");
      }

      found_plan_instruction = true;
      start_waypoint = plan_instruction->getWaypoint();
    }
  }

  // Setup Basic Info
  pci->basic_info.n_steps = index;
  pci->basic_info.manip = request.manipulator;
  pci->basic_info.start_fixed = false;
  pci->basic_info.use_time = false;
  //  pci->basic_info.convex_solver = optimizer;  // TODO: Fix this when port to trajopt_ifopt

  // Set trajopt seed
  assert(static_cast<long>(seed_states.size()) == pci->basic_info.n_steps);
  pci->init_info.type = trajopt::InitInfo::GIVEN_TRAJ;
  pci->init_info.data.resize(pci->basic_info.n_steps, pci->kin->numJoints());
  for (long i = 0; i < pci->basic_info.n_steps; ++i)
    pci->init_info.data.row(i) = seed_states[static_cast<std::size_t>(i)];

  // Apply Composite Profile
  std::string profile = request.instructions.getProfile();
  if (profile.empty())
    profile = "DEFAULT";

  TrajOptCompositeProfile::Ptr cur_composite_profile{ nullptr };
  auto it = composite_profiles.find(profile);
  if (it == composite_profiles.end())
    cur_composite_profile = std::make_shared<TrajOptDefaultCompositeProfile>();
  else
    cur_composite_profile = it->second;

  cur_composite_profile->apply(*pci, 0, pci->basic_info.n_steps - 1, active_links, fixed_steps);

  // Construct Problem
  trajopt::TrajOptProb::Ptr problem = trajopt::ConstructProblem(*pci);

  return problem;
}

}  // namespace tesseract_planning
#endif