/**
 * @file discrete_contact_check_task.h
 * @brief Discrete Collision check trajectory task
 *
 * @author Levi Armstrong
 * @date August 10. 2020
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
#ifndef TESSERACT_TASK_COMPOSER_DISCRETE_CONTACT_CHECK_TASK_H
#define TESSERACT_TASK_COMPOSER_DISCRETE_CONTACT_CHECK_TASK_H
#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <vector>
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_task_composer/core/task_composer_task.h>
#include <tesseract_task_composer/core/task_composer_node_info.h>

#include <tesseract_environment/fwd.h>
#include <tesseract_collision/core/types.h>

namespace tesseract_planning
{
class TaskComposerPluginFactory;
class DiscreteContactCheckTask : public TaskComposerTask
{
public:
  // Requried
  static const std::string INPUT_PROGRAM_PORT;
  static const std::string INPUT_ENVIRONMENT_PORT;
  static const std::string INPUT_PROFILES_PORT;

  // Optional
  static const std::string INPUT_MANIP_INFO_PORT;
  static const std::string INPUT_COMPOSITE_PROFILE_REMAPPING_PORT;

  using Ptr = std::shared_ptr<DiscreteContactCheckTask>;
  using ConstPtr = std::shared_ptr<const DiscreteContactCheckTask>;
  using UPtr = std::unique_ptr<DiscreteContactCheckTask>;
  using ConstUPtr = std::unique_ptr<const DiscreteContactCheckTask>;

  DiscreteContactCheckTask();
  explicit DiscreteContactCheckTask(std::string name,
                                    std::string input_program_key,
                                    std::string input_environment_key,
                                    std::string input_profiles_key,
                                    bool conditional = true);
  explicit DiscreteContactCheckTask(std::string name,
                                    const YAML::Node& config,
                                    const TaskComposerPluginFactory& plugin_factory);

  ~DiscreteContactCheckTask() override = default;
  DiscreteContactCheckTask(const DiscreteContactCheckTask&) = delete;
  DiscreteContactCheckTask& operator=(const DiscreteContactCheckTask&) = delete;
  DiscreteContactCheckTask(DiscreteContactCheckTask&&) = delete;
  DiscreteContactCheckTask& operator=(DiscreteContactCheckTask&&) = delete;

  bool operator==(const DiscreteContactCheckTask& rhs) const;
  bool operator!=(const DiscreteContactCheckTask& rhs) const;

protected:
  friend struct tesseract_common::Serialization;
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);  // NOLINT

  static TaskComposerNodePorts ports();

  std::unique_ptr<TaskComposerNodeInfo>
  runImpl(TaskComposerContext& context, OptionalTaskComposerExecutor executor = std::nullopt) const override final;
};

class DiscreteContactCheckTaskInfo : public TaskComposerNodeInfo
{
public:
  using Ptr = std::shared_ptr<DiscreteContactCheckTaskInfo>;
  using ConstPtr = std::shared_ptr<const DiscreteContactCheckTaskInfo>;
  using UPtr = std::unique_ptr<DiscreteContactCheckTaskInfo>;
  using ConstUPtr = std::unique_ptr<const DiscreteContactCheckTaskInfo>;

  DiscreteContactCheckTaskInfo() = default;
  DiscreteContactCheckTaskInfo(const TaskComposerNodeInfo& task);

  std::shared_ptr<const tesseract_environment::Environment> env;
  std::vector<tesseract_collision::ContactResultMap> contact_results;

  std::unique_ptr<TaskComposerNodeInfo> clone() const override;

  bool operator==(const DiscreteContactCheckTaskInfo& rhs) const;
  bool operator!=(const DiscreteContactCheckTaskInfo& rhs) const;

private:
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);  // NOLINT
};

}  // namespace tesseract_planning

BOOST_CLASS_EXPORT_KEY(tesseract_planning::DiscreteContactCheckTask)
BOOST_CLASS_EXPORT_KEY(tesseract_planning::DiscreteContactCheckTaskInfo)
#endif  // TESSERACT_TASK_COMPOSER_DISCRETE_CONTACT_CHECK_TASK_H
