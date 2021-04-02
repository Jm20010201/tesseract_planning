/**
 * @file wait_instruction.h
 * @brief
 *
 * @author Levi Armstrong
 * @date November 15, 2020
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
#ifndef TESSERACT_COMMAND_LANGUAGE_WAIT_INSTRUCTION_H
#define TESSERACT_COMMAND_LANGUAGE_WAIT_INSTRUCTION_H

#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <iostream>
#include <string>
#include <tinyxml2.h>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/nvp.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_command_language/instruction_type.h>
#include <tesseract_common/utils.h>

namespace tesseract_planning
{
enum class WaitInstructionType : int
{
  TIME = 0,
  DIGITAL_INPUT_HIGH = 1,
  DIGITAL_INPUT_LOW = 2,
  DIGITAL_OUTPUT_HIGH = 3,
  DIGITAL_OUTPUT_LOW = 4
};

/**
 * @brief This is a wait instruction similar to wait instruction on industrial controllers.
 * @details The instruction has several modes of operation.
 *
 *   - TIME                : This will wait for a specified number of seconds and then continue
 *   - DIGITAL_INPUT_HIGH  : This will wait for a digital input to go high(1) then continue
 *   - DIGITAL_INPUT_LOW   : This will wait for a digital input to go low(0) then continue
 *   - DIGITAL_OUTPUT_HIGH : This will wait for a digital output to go high(1) then continue
 *   - DIGITAL_OUTPUT_LOW  : This will wait for a digital output to go low(0) then continue
 */
class WaitInstruction
{
public:
  WaitInstruction() = default;  // Required for boost serialization do not use
  WaitInstruction(double time) : wait_type_(WaitInstructionType::TIME), wait_time_(time) {}
  WaitInstruction(WaitInstructionType type, int io) : wait_type_(type), wait_io_(io)
  {
    if (wait_type_ == WaitInstructionType::TIME)
      throw std::runtime_error("WaitInstruction: Invalid type 'WaitInstructionType::TIME' for constructor");
  }

  const std::string& getDescription() const { return description_; }

  void setDescription(const std::string& description) { description_ = description; }

  void print(const std::string& prefix = "") const  // NOLINT
  {
    std::cout << prefix + "Wait Instruction, Wait Type: " << static_cast<int>(wait_type_);
    std::cout << ", Description: " << getDescription() << std::endl;
  }

  /**
   * @brief Get the wait type
   * @return The wait type
   */
  WaitInstructionType getWaitType() const { return wait_type_; }

  /**
   * @brief Set the wait type
   * @param type The wait type
   */
  void setWaitType(WaitInstructionType type) { wait_type_ = type; }

  /**
   * @brief Get wait time in second
   * @return The wait time in second
   */
  double getWaitTime() const { return wait_time_; }
  /**
   * @brief Set wait time in second
   * @param time The wait time in second
   */
  void setWaitTime(double time) { wait_time_ = time; }

  /**
   * @brief Get the wait IO
   * @return The wait IO
   */
  int getWaitIO() const { return wait_io_; }

  /**
   * @brief Set the wait IO
   * @param io The wait IO
   */
  void setWaitIO(int io) { wait_io_ = io; }

  /**
   * @brief Equal operator. Does not compare descriptions
   * @param rhs TimerInstruction
   * @return True if equal, otherwise false
   */
  bool operator==(const WaitInstruction& rhs) const
  {
    static auto max_diff = static_cast<double>(std::numeric_limits<float>::epsilon());

    bool equal = true;
    equal &= tesseract_common::almostEqualRelativeAndAbs(wait_time_, rhs.wait_time_, max_diff);
    equal &= (wait_type_ == rhs.wait_type_);
    equal &= (wait_io_ == rhs.wait_io_);

    return equal;
  }

  /**
   * @brief Not equal operator. Does not compare descriptions
   * @param rhs TimerInstruction
   * @return True if not equal, otherwise false
   */
  bool operator!=(const WaitInstruction& rhs) const { return !operator==(rhs); }

private:
  /** @brief The description of the instruction */
  std::string description_{ "Tesseract Wait Instruction" };
  WaitInstructionType wait_type_;
  double wait_time_{ 0 };
  int wait_io_{ -1 };

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /*version*/)
  {
    ar& boost::serialization::make_nvp("description", description_);
    ar& boost::serialization::make_nvp("wait_type", wait_type_);
    ar& boost::serialization::make_nvp("wait_time", wait_time_);
    ar& boost::serialization::make_nvp("wait_io", wait_io_);
  }
};
}  // namespace tesseract_planning

#ifdef SWIG
%tesseract_command_language_add_instruction_type(WaitInstruction)
#endif  // SWIG

#endif  // TESSERACT_COMMAND_LANGUAGE_WAIT_INSTRUCTION_H
