﻿/**
 * @file raster_taskflow.cpp
 * @brief Plans raster paths
 *
 * @author Matthew Powelson
 * @date July 15, 2020
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
#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <functional>
#include <taskflow/taskflow.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_process_managers/taskflow_generators/raster_only_taskflow.h>

#include <tesseract_command_language/instruction_type.h>
#include <tesseract_command_language/composite_instruction.h>
#include <tesseract_command_language/plan_instruction.h>
#include <tesseract_command_language/utils/get_instruction_utils.h>

#include <tesseract_common/utils.h>

using namespace tesseract_planning;

RasterOnlyTaskflow::RasterOnlyTaskflow(TaskflowGenerator::UPtr transition_taskflow_generator,
                                       TaskflowGenerator::UPtr raster_taskflow_generator,
                                       std::string name)
  : transition_taskflow_generator_(std::move(transition_taskflow_generator))
  , raster_taskflow_generator_(std::move(raster_taskflow_generator))
  , name_(name)
  , taskflow_(name)
{
}

const std::string& RasterOnlyTaskflow::getName() const { return name_; }

tf::Taskflow& RasterOnlyTaskflow::generateTaskflow(ProcessInput input,
                                                   std::function<void()> done_cb,
                                                   std::function<void()> error_cb)
{
  // This should make all of the isComposite checks so that you can safely cast below
  if (!checkProcessInput(input))
  {
    CONSOLE_BRIDGE_logError("Invalid Process Input");
    throw std::runtime_error("Invalid Process Input");
  }

  // Clear the process manager
  clear();

  // Store the current size of the tasks so that we can add from_start later
  std::size_t starting_raster_idx = raster_tasks_.size();

  // Generate all of the raster tasks. They don't depend on anything
  const Instruction* input_instruction = input.getInstruction();
  for (std::size_t idx = 0; idx < input.size(); idx += 2)
  {
    // Get Start Plan Instruction
    Instruction start_instruction = NullInstruction();
    if (idx == 0)
    {
      assert(isCompositeInstruction(*input_instruction));
      start_instruction = input_instruction->cast_const<CompositeInstruction>()->getStartInstruction();
    }
    else
    {
      ProcessInput pre_input = input[idx - 1];
      const Instruction* pre_input_instruction = pre_input.getInstruction();
      assert(isCompositeInstruction(*pre_input_instruction));
      const auto* tci = pre_input_instruction->cast_const<CompositeInstruction>();
      auto* li = getLastPlanInstruction(*tci);
      assert(li != nullptr);
      start_instruction = *li;
    }

    start_instruction.cast<PlanInstruction>()->setPlanType(PlanInstructionType::START);
    ProcessInput raster_input = input[idx];
    raster_input.setStartInstruction(start_instruction);
    auto raster_step =
        taskflow_
            .composed_of(
                raster_taskflow_generator_->generateTaskflow(raster_input,
                                                             std::bind(&RasterOnlyTaskflow::successCallback,
                                                                       this,
                                                                       raster_input.getInstruction()->getDescription(),
                                                                       done_cb),
                                                             std::bind(&RasterOnlyTaskflow::failureCallback,
                                                                       this,
                                                                       raster_input.getInstruction()->getDescription(),
                                                                       error_cb)))
            .name("Raster #" + std::to_string(idx / 2) + ": " + raster_input.getInstruction()->getDescription());
    raster_tasks_.push_back(raster_step);
  }

  // Loop over all transitions
  std::size_t transition_idx = 0;
  for (std::size_t input_idx = 1; input_idx < input.size() - 1; input_idx += 2)
  {
    // This use to extract the start and end, but things were changed so the seed is generated as part of the
    // taskflow. So the seed is only a skeleton and does not contain move instructions. So instead we provide the
    // composite and let the generateTaskflow extract the start and end waypoint from the composite. This is also more
    // robust because planners could modify composite size, which is rare but does happen when using OMPL where it is
    // not possible to simplify the trajectory to the desired number of states.
    ProcessInput transition_input = input[input_idx];
    transition_input.setStartInstruction(std::vector<std::size_t>({ input_idx - 1 }));
    transition_input.setEndInstruction(std::vector<std::size_t>({ input_idx + 1 }));
    auto transition_step = taskflow_
                               .composed_of(transition_taskflow_generator_->generateTaskflow(
                                   transition_input,
                                   std::bind(&RasterOnlyTaskflow::successCallback,
                                             this,
                                             transition_input.getInstruction()->getDescription(),
                                             done_cb),
                                   std::bind(&RasterOnlyTaskflow::failureCallback,
                                             this,
                                             transition_input.getInstruction()->getDescription(),
                                             error_cb)))
                               .name("Transition #" + std::to_string(transition_idx) + ": " +
                                     transition_input.getInstruction()->getDescription());

    // Each transition is independent and thus depends only on the adjacent rasters
    transition_step.succeed(raster_tasks_[starting_raster_idx + transition_idx]);
    transition_step.succeed(raster_tasks_[starting_raster_idx + transition_idx + 1]);

    transition_tasks_.push_back(transition_step);
    transition_idx++;
  }

  return taskflow_;
}

void RasterOnlyTaskflow::abort()
{
  transition_taskflow_generator_->abort();
  raster_taskflow_generator_->abort();

  CONSOLE_BRIDGE_logError("Terminating Taskflow");
}

void RasterOnlyTaskflow::reset()
{
  transition_taskflow_generator_->reset();
  raster_taskflow_generator_->reset();
}

void RasterOnlyTaskflow::clear()

{
  transition_taskflow_generator_->clear();
  raster_taskflow_generator_->clear();
  taskflow_.clear();
  raster_tasks_.clear();
}

bool RasterOnlyTaskflow::checkProcessInput(const tesseract_planning::ProcessInput& input) const
{
  // -------------
  // Check Input
  // -------------
  if (!input.tesseract)
  {
    CONSOLE_BRIDGE_logError("ProcessInput tesseract is a nullptr");
    return false;
  }

  // Check the overall input
  const Instruction* input_instruction = input.getInstruction();
  if (!isCompositeInstruction(*input_instruction))
  {
    CONSOLE_BRIDGE_logError("ProcessInput Invalid: input.instructions should be a composite");
    return false;
  }
  const auto* composite = input_instruction->cast_const<CompositeInstruction>();

  // Check that it has a start instruction
  if (!composite->hasStartInstruction() && isNullInstruction(input.getStartInstruction()))
  {
    CONSOLE_BRIDGE_logError("ProcessInput Invalid: input.instructions should have a start instruction");
    return false;
  }

  // Check rasters and transitions
  for (std::size_t index = 0; index < composite->size(); index++)
  {
    // Both rasters and transitions should be a composite
    if (!isCompositeInstruction(composite->at(index)))
    {
      CONSOLE_BRIDGE_logError("ProcessInput Invalid: Both rasters and transitions should be a composite");
      return false;
    }
  }

  return true;
}

void RasterOnlyTaskflow::successCallback(std::string message, std::function<void()> user_callback)
{
  CONSOLE_BRIDGE_logInform("%s Successful: %s", name_.c_str(), message.c_str());
  if (user_callback)
    user_callback();
}

void RasterOnlyTaskflow::failureCallback(std::string message, std::function<void()> user_callback)
{
  // For this process, any failure of a sub-TaskFlow indicates a planning failure. Abort all future tasks
  transition_taskflow_generator_->abort();
  raster_taskflow_generator_->abort();

  // Print an error if this is the first failure
  CONSOLE_BRIDGE_logError("%s Failure: %s", name_.c_str(), message.c_str());
  if (user_callback)
    user_callback();
}