/*
 * Copyright (C) 2021 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <memory>
#include <mutex>
#include <string>

#include <ignition/common/Console.hh>
#include <ignition/gui/Application.hh>
#include <ignition/gui/GuiEvents.hh>
#include <ignition/gui/MainWindow.hh>
#include <ignition/plugin/Register.hh>
#include <ignition/transport.hh>

#include "ignition/gazebo/components/Name.hh"
#include "ignition/gazebo/gui/GuiEvents.hh"

#include "CopyPaste.hh"

namespace ignition::gazebo
{
  class CopyPastePrivate
  {
    /// \brief The entity that is currently selected
    public: Entity selectedEntity = kNullEntity;

    /// \brief The name of the entity that is currently selected
    public: std::string selectedEntityName = "";

    /// \brief The name of the entity that is copied
    public: std::string copiedData = "";

    /// \brief Transport node for handling service calls
    public: transport::Node node;

    /// \brief Name of the copy service
    public: const std::string copyService = "/gui/copy";

    /// \brief Name of the paste service
    public: const std::string pasteService = "/gui/paste";

    /// \brief A mutex to ensure that there are no race conditions between
    /// copy/paste
    public: std::mutex mutex;
  };
}

using namespace ignition;
using namespace gazebo;

/////////////////////////////////////////////////
CopyPaste::CopyPaste()
  : GuiSystem(), dataPtr(std::make_unique<CopyPastePrivate>())
{
  if (!this->dataPtr->node.Advertise(this->dataPtr->copyService,
        &CopyPaste::CopyServiceCB, this))
  {
    ignerr << "Error advertising service [" << this->dataPtr->copyService
           << "]" << std::endl;
  }

  if (!this->dataPtr->node.Advertise(this->dataPtr->pasteService,
        &CopyPaste::PasteServiceCB, this))
  {
    ignerr << "Error advertising service [" << this->dataPtr->pasteService
           << "]" << std::endl;
  }
}

/////////////////////////////////////////////////
CopyPaste::~CopyPaste() = default;

/////////////////////////////////////////////////
void CopyPaste::LoadConfig(const tinyxml2::XMLElement *)
{
  if (this->title.empty())
    this->title = "Copy/Paste";

  ignition::gui::App()->findChild<
      ignition::gui::MainWindow *>()->installEventFilter(this);
  ignition::gui::App()->findChild<
      ignition::gui::MainWindow *>()->QuickWindow()->installEventFilter(this);
}

/////////////////////////////////////////////////
void CopyPaste::Update(const UpdateInfo &/*_info*/,
    EntityComponentManager &_ecm)
{
  std::lock_guard<std::mutex> guard(this->dataPtr->mutex);
  auto nameComp =
    _ecm.Component<components::Name>(this->dataPtr->selectedEntity);
  if (!nameComp)
    return;
  this->dataPtr->selectedEntityName = nameComp->Data();
}

/////////////////////////////////////////////////
void CopyPaste::OnCopy()
{
  std::lock_guard<std::mutex> guard(this->dataPtr->mutex);
  this->dataPtr->copiedData = this->dataPtr->selectedEntityName;
}

/////////////////////////////////////////////////
void CopyPaste::OnPaste()
{
  std::lock_guard<std::mutex> guard(this->dataPtr->mutex);

  // we should only paste if something has been copied
  if (!this->dataPtr->copiedData.empty())
  {
    ignition::gui::events::SpawnCloneFromName event(this->dataPtr->copiedData);
    ignition::gui::App()->sendEvent(
      ignition::gui::App()->findChild<ignition::gui::MainWindow *>(),
      &event);
  }
}

/////////////////////////////////////////////////
bool CopyPaste::eventFilter(QObject *_obj, QEvent *_event)
{
  if (_event->type() == ignition::gazebo::gui::events::EntitiesSelected::kType)
  {
    std::lock_guard<std::mutex> guard(this->dataPtr->mutex);

    auto selectedEvent =
        reinterpret_cast<gui::events::EntitiesSelected *>(_event);
    if (selectedEvent && (selectedEvent->Data().size() == 1u))
      this->dataPtr->selectedEntity = selectedEvent->Data()[0];
  }
  if (_event->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(_event);
    if (keyEvent && keyEvent->matches(QKeySequence::Copy))
    {
      this->OnCopy();
    }
    else if (keyEvent && keyEvent->matches(QKeySequence::Paste))
    {
      this->OnPaste();
    }
  }

  // Standard event processing
  return QObject::eventFilter(_obj, _event);
}

/////////////////////////////////////////////////
bool CopyPaste::CopyServiceCB(const ignition::msgs::StringMsg &_req,
    ignition::msgs::Boolean &_resp)
{
  {
    std::lock_guard<std::mutex> guard(this->dataPtr->mutex);
    this->dataPtr->copiedData = _req.data();
  }
  _resp.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool CopyPaste::PasteServiceCB(const ignition::msgs::Empty &/*_req*/,
    ignition::msgs::Boolean &_resp)
{
  this->OnPaste();
  _resp.set_data(true);
  return true;
}

// Register this plugin
IGNITION_ADD_PLUGIN(ignition::gazebo::CopyPaste,
                    ignition::gui::Plugin)
