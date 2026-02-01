#include <Error.h>
#include <Input.h>
#include <Temporal.h>
#include <VarkorMain.h>
#include <comp/BoxCollider.h>
#include <comp/Camera.h>
#include <comp/CameraOrbiter.h>
#include <comp/Name.h>
#include <comp/Relationship.h>
#include <comp/Sprite.h>
#include <comp/Text.h>
#include <comp/Transform.h>
#include <gfx/Renderer.h>
#include <imgui/imgui.h>
#include <math/Constants.h>
#include <string>
#include <world/Registrar.h>
#include <world/World.h>

// This is a cellular automata type game. There are three primary game elements.

// Requirement - A requirment occupies 2 cells, both of which can be anywhere on
// the grid. One of the cells signifies a physical digit. The other cell
// signifies the filtered digit.

// Shifter - An arrow pointing in one of four directions. When a physical digit
// arrives at a shifter, the physical digit begins to move in the direction the
// arrow points.

// Filter - When a physical digit arrives at the cell occupied by a filter, the
// filter changes the digit, e.g. +1, *2, -5.

// The goal is to place a set of filters and shifters, such that the physical
// digits arrive at the filtered digits with the same values.

enum class Direction { Up, Right, Down, Left };

void LevelSetup(size_t levelIdx);
bool nPaused = true;
bool nAutomataStarted = false;
const float nStartTime = 0.9f;
float nAutomataTimePassed = nStartTime;
const Vec3 nFieldOrigin = {0.0f, 0.0f, 0.0f};
constexpr int nFieldWidth = 10;
constexpr int nFieldHeight = 10;
World::MemberId nDigitLayer[nFieldWidth][nFieldHeight];
World::MemberId nModifierLayer[nFieldWidth][nFieldHeight];

const float nCursorZ = -1.0f;
const float nFieldZ = 0.0f;
const float nModifierZ = 1.0f;
const float nDigitZ = 2.0f;

const float nModifierScale = 0.7f;
const float nPlaceableScale = 1.2f;
const float nDigitScale = 0.6f;

const Vec3 nPlaceableIdsOrigin = {12.0f, 7.0f, nModifierZ};
Ds::Vector<World::MemberId> nPlaceableIds;
const int nPlaceableCols = 6;

struct Cursor {
  World::Object mObject;
  World::Object mSelectedObject;
  bool mInField;
  bool mPlaceableSelected;
  int mCell[2];
  int mPlaceableCell[2];
};
Cursor nCursor;

World::Object nRunDisplay;
const char* nRunDisplayStartText = " =";

struct Digit {
  int mCell[2];
  int mValue;
  Direction mDirection;
};

struct Filter {
  int mStartCell[2];
  int mValue;
  enum class Type { Add, Sub, Mul, Mod };
  Type mType;
  bool mPlaceable;
};

struct Shifter {
  int mStartCell[2];
  Direction mDirection;
  bool mPlaceable;
};

struct Level {
  Ds::Vector<Digit> mDigits;
  Ds::Vector<Filter> mFilters;
  Ds::Vector<Shifter> mShifters;
};
Ds::Vector<Level> nLevels;
size_t nCurrentLevel = 0;
void CreateLevels() {
  {
    Level level;
    level.mDigits = {
      {
        .mCell = {5, 5},
        .mValue = 0,
        .mDirection = Direction::Up,
      },
      {
        .mCell = {6, 4},
        .mValue = 1,
        .mDirection = Direction::Right,
      },
      {
        .mCell = {5, 3},
        .mValue = 2,
        .mDirection = Direction::Down,
      },
      {
        .mCell = {4, 4},
        .mValue = 3,
        .mDirection = Direction::Left,
      },
    };
    level.mFilters = {
      {
        .mStartCell = {-1, -1},
        .mValue = 3,
        .mType = Filter::Type::Add,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mValue = 1,
        .mType = Filter::Type::Sub,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mValue = 3,
        .mType = Filter::Type::Mul,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mValue = 3,
        .mType = Filter::Type::Mod,
        .mPlaceable = true,
      },
    };
    level.mShifters = {
      {
        .mStartCell = {-1, -1},
        .mDirection = Direction::Down,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mDirection = Direction::Left,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mDirection = Direction::Up,
        .mPlaceable = true,
      },
      {
        .mStartCell = {-1, -1},
        .mDirection = Direction::Right,
        .mPlaceable = true,
      },
    };
    nLevels.Emplace(std::move(level));
  }
}

void InitializeLayers() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      nDigitLayer[j][i] = World::nInvalidMemberId;
      nModifierLayer[j][i] = World::nInvalidMemberId;
    }
  }
}

void UpdatePlaceableGraphics() {
  World::Space& space = World::nLayers.Back()->mSpace;
  for (size_t i = 0; i < nPlaceableIds.Size(); ++i) {
    auto& transform = space.Get<Comp::Transform>(nPlaceableIds[i]);
    Vec3 offset = {
      (float)(i % nPlaceableCols), -(float)(i / nPlaceableCols), nModifierZ};
    transform.SetTranslation(nPlaceableIdsOrigin + offset);
  }
}

void UpdateGraphics() {
  World::Space& space = World::nLayers.Back()->mSpace;
  Ds::Vector<MemberId> digitIds = space.Slice<Digit>();
  for (MemberId memberId: digitIds) {
    const auto& digit = space.Get<Digit>(memberId);
    auto& transform = space.Get<Comp::Transform>(memberId);
    Vec3 offset = {(float)digit.mCell[0], (float)digit.mCell[1], nDigitZ};
    transform.SetTranslation(nFieldOrigin + offset);

    const auto& relationship = space.Get<Comp::Relationship>(memberId);
    auto& text = space.Get<Comp::Text>(relationship.mChildren[0]);
    text.mText = std::to_string(digit.mValue);
  }
}

void PerformStep() {
  World::Space& space = World::nLayers.Back()->mSpace;
  Ds::Vector<MemberId> digitIds = space.Slice<Digit>();
  for (MemberId memberId: digitIds) {
    // Update the digits position
    auto& digit = space.Get<Digit>(memberId);
    nDigitLayer[digit.mCell[0]][digit.mCell[1]] = World::nInvalidMemberId;
    switch (digit.mDirection) {
    case Direction::Up: digit.mCell[1] += 1; break;
    case Direction::Right: digit.mCell[0] += 1; break;
    case Direction::Down: digit.mCell[1] -= 1; break;
    case Direction::Left: digit.mCell[0] -= 1; break;
    }
    digit.mCell[0] = Math::Clamp(0, 9, digit.mCell[0]);
    digit.mCell[1] = Math::Clamp(0, 9, digit.mCell[1]);

    World::MemberId modifierMemberId =
      nModifierLayer[digit.mCell[0]][digit.mCell[1]];
    if (modifierMemberId == World::nInvalidMemberId) {
      continue;
    }
    World::Object modifierObj(&space, modifierMemberId);
    auto* shifter = modifierObj.TryGet<Shifter>();
    if (shifter != nullptr) {
      digit.mDirection = shifter->mDirection;
    }
    auto* filter = modifierObj.TryGet<Filter>();
    if (filter != nullptr) {
      switch (filter->mType) {
      case Filter::Type::Add:
        digit.mValue = digit.mValue + filter->mValue;
        break;
      case Filter::Type::Sub:
        digit.mValue = digit.mValue - filter->mValue;
        break;
      case Filter::Type::Mul:
        digit.mValue = digit.mValue * filter->mValue;
        break;
      case Filter::Type::Mod:
        digit.mValue = digit.mValue % filter->mValue;
        break;
      }
      digit.mValue = (digit.mValue + 10) % 10;
    }
  }
}

void RunAutomata() {
  int prevTimePassedFloor = (int)nAutomataTimePassed;
  nAutomataTimePassed += Temporal::DeltaTime();
  int currTimePassedFloor = (int)nAutomataTimePassed;
  if (prevTimePassedFloor != currTimePassedFloor) {
    PerformStep();
    UpdateGraphics();
  }
}

void TryRemoveModifierFromField(World::MemberId memberId) {
  World::Space& space = World::nLayers.Back()->mSpace;
  if (memberId != World::nInvalidMemberId) {
    auto* filter = space.TryGet<Filter>(memberId);
    if (filter != nullptr && !filter->mPlaceable) {
      return;
    }
    auto* shifter = space.TryGet<Shifter>(memberId);
    if (shifter != nullptr && !shifter->mPlaceable) {
      return;
    }
    nPlaceableIds.Push(memberId);
    nModifierLayer[nCursor.mCell[0]][nCursor.mCell[1]] =
      World::nInvalidMemberId;
  }
}

void RunPlaceMode() {
  if (Input::KeyPressed(Input::Key::S)) {
    nCursor.mInField = !nCursor.mInField;
    nCursor.mPlaceableSelected = false;
    nCursor.mSelectedObject.Get<Comp::Sprite>().mVisible = false;
  }

  // Handle placement and removal of field modifiers
  World::Space& space = World::nLayers.Back()->mSpace;
  if (Input::KeyPressed(Input::Key::D)) {
    if (!nCursor.mInField) {
      nCursor.mInField = true;
      nCursor.mPlaceableSelected = true;
      Vec3 offset = {
        (float)nCursor.mPlaceableCell[0],
        -(float)nCursor.mPlaceableCell[1],
        nCursorZ};
      auto& selectedTransform = nCursor.mSelectedObject.Get<Comp::Transform>();
      selectedTransform.SetTranslation(nPlaceableIdsOrigin + offset);
      nCursor.mSelectedObject.Get<Comp::Sprite>().mVisible = true;
    }
    else {
      World::MemberId modifierIdUnderCursor =
        nModifierLayer[nCursor.mCell[0]][nCursor.mCell[1]];
      TryRemoveModifierFromField(modifierIdUnderCursor);
      if (nCursor.mPlaceableSelected) {
        size_t placeableIndex =
          nCursor.mPlaceableCell[0] + (nCursor.mPlaceableCell[1] * 6);
        World::MemberId placeableId = nPlaceableIds[placeableIndex];
        nPlaceableIds.Remove(placeableIndex);
        auto& placeableTransform = space.Get<Comp::Transform>(placeableId);
        Vec3 offset = {
          (float)nCursor.mCell[0], (float)nCursor.mCell[1], nModifierZ};
        placeableTransform.SetTranslation(nFieldOrigin + offset);
        nModifierLayer[nCursor.mCell[0]][nCursor.mCell[1]] = placeableId;
        nCursor.mPlaceableSelected = false;
        nCursor.mSelectedObject.Get<Comp::Sprite>().mVisible = false;
      }
      UpdatePlaceableGraphics();
    }
  }

  // Handle cursor movement.
  int direction[2] = {0, 0};
  if (Input::KeyPressed(Input::Key::Up)) {
    direction[1] = 1;
  }
  if (Input::KeyPressed(Input::Key::Right)) {
    direction[0] = 1;
  }
  if (Input::KeyPressed(Input::Key::Down)) {
    direction[1] = -1;
  }
  if (Input::KeyPressed(Input::Key::Left)) {
    direction[0] = -1;
  }

  if (nCursor.mInField) {
    nCursor.mCell[0] = (nCursor.mCell[0] + direction[0] + 10) % 10;
    nCursor.mCell[1] = (nCursor.mCell[1] + direction[1] + 10) % 10;
  }
  else {
    nCursor.mPlaceableCell[0] += direction[0];
    nCursor.mPlaceableCell[1] += direction[1];
    // Handle column wrapping
    int lastCol = (nPlaceableIds.Size() - 1) % nPlaceableCols;
    int lastRow = (nPlaceableIds.Size() - 1) / nPlaceableCols;
    if (nCursor.mPlaceableCell[1] == lastRow) {
      nCursor.mPlaceableCell[0] =
        (nCursor.mPlaceableCell[0] + (lastCol + 1)) % (lastCol + 1);
    }
    else {
      nCursor.mPlaceableCell[0] =
        (nCursor.mPlaceableCell[0] + nPlaceableCols) % nPlaceableCols;
    }
    // Handle row wrapping
    if (nCursor.mPlaceableCell[0] <= lastCol) {
      nCursor.mPlaceableCell[1] =
        (nCursor.mPlaceableCell[1] + (lastRow + 1)) % (lastRow + 1);
    }
    else {
      nCursor.mPlaceableCell[1] =
        (nCursor.mPlaceableCell[1] + lastRow) % lastRow;
    }
  }

  auto& cursorTransform = nCursor.mObject.Get<Comp::Transform>();
  if (nCursor.mInField) {
    Vec3 offset = {(float)nCursor.mCell[0], (float)nCursor.mCell[1], nCursorZ};
    cursorTransform.SetTranslation(nFieldOrigin + offset);
  }
  else {
    Vec3 offset = {
      (float)nCursor.mPlaceableCell[0],
      -(float)nCursor.mPlaceableCell[1],
      nCursorZ};
    cursorTransform.SetTranslation(nPlaceableIdsOrigin + offset);
  }
}

void CentralUpdate() {
  if (Input::KeyPressed(Input::Key::Space)) {
    nPaused = !nPaused;
    if (nPaused) {
      nRunDisplay.Get<Comp::Text>().mText = "o=";
      nAutomataTimePassed = (float)(int)nAutomataTimePassed + 0.9f;
    }
    else {
      nAutomataStarted = true;
      nRunDisplay.Get<Comp::Text>().mText = "o>";
      nCursor.mObject.Get<Comp::Sprite>().mVisible = false;
      nCursor.mSelectedObject.Get<Comp::Sprite>().mVisible = false;
    }
  }

  if (!nPaused) {
    RunAutomata();
  }
  else if (!nAutomataStarted) {
    RunPlaceMode();
  }

  if (Input::KeyPressed(Input::Key::R)) {
    nPaused = true;
    nAutomataStarted = false;
    nAutomataTimePassed = nStartTime;
    nRunDisplay.Get<Comp::Text>().mText = nRunDisplayStartText;
    nCursor.mObject.Get<Comp::Sprite>().mVisible = true;
    nCursor.mSelectedObject.Get<Comp::Sprite>().mVisible = false;
    LevelSetup(nCurrentLevel);
  }
}

void FieldSetup() {
  Gfx::Renderer::nClearColor = {0.02f, 0.02f, 0.02f, 1.0};

  World::LayerIt layerIt = World::nLayers.EmplaceBack("Field");
  World::Space& space = layerIt->mSpace;

  World::Object field = space.CreateObject();
  auto& fieldTransform = field.Add<Comp::Transform>();
  fieldTransform.SetTranslation({0.0f, 0.0f, 0.0f});
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      World::Object gridSquare = field.CreateChild();
      auto& squareTransform = gridSquare.Add<Comp::Transform>();
      Vec3 offset = {(float)j, (float)i, nFieldZ};
      squareTransform.SetTranslation(nFieldOrigin + offset);
      squareTransform.SetUniformScale(0.8f);
      auto& squareSprite = gridSquare.Add<Comp::Sprite>();
      squareSprite.mMaterialId = "images:GridMaterial";
    }
  }

  nRunDisplay = field.CreateChild();
  auto& runDisplayTransform = nRunDisplay.Add<Comp::Transform>();
  runDisplayTransform.SetTranslation({14.5f, 8.0f, 0.0f});
  runDisplayTransform.SetUniformScale(2.0f);
  auto& runDisplayText = nRunDisplay.Add<Comp::Text>();
  runDisplayText.mColor = {1.0f, 1.0f, 1.0f, 1.0f};
  runDisplayText.mAlign = Comp::Text::Alignment::Center;
  runDisplayText.mText = nRunDisplayStartText;

  nCursor.mObject = space.CreateObject();
  auto& cursorTransform = nCursor.mObject.Add<Comp::Transform>();
  Vec3 offset = {0.0f, 0.0f, nCursorZ};
  cursorTransform.SetTranslation(nFieldOrigin + offset);
  nCursor.mInField = true;
  nCursor.mPlaceableSelected = false;
  nCursor.mCell[0] = 0;
  nCursor.mCell[1] = 0;
  nCursor.mPlaceableCell[0] = 0;
  nCursor.mPlaceableCell[1] = 0;
  auto& cursorSprite = nCursor.mObject.Add<Comp::Sprite>();
  cursorSprite.mMaterialId = "images:Cursor";

  nCursor.mSelectedObject = space.CreateObject();
  auto& selectedTransform = nCursor.mSelectedObject.Add<Comp::Transform>();
  offset = {0.0f, 0.0f, nCursorZ};
  selectedTransform.SetTranslation(nPlaceableIdsOrigin + offset);
  auto& selectedSprite = nCursor.mSelectedObject.Add<Comp::Sprite>();
  selectedSprite.mMaterialId = "images:Selected";
  selectedSprite.mVisible = false;

  World::Object camera = space.CreateObject();
  auto& cameraComp = camera.Add<Comp::Camera>();
  cameraComp.mProjectionType = Comp::Camera::ProjectionType::Orthographic;
  cameraComp.mHeight = 11.0f;
  layerIt->mCameraId = camera.mMemberId;
  auto& cameraTransform = camera.Get<Comp::Transform>();
  cameraTransform.SetTranslation({9.0f, 4.5f, 3.0f});
}

void MakeLevelEmpty() {
  World::Space& space = World::nLayers.Back()->mSpace;
  Ds::Vector<MemberId> digitMemberIds = space.Slice<Digit>();
  for (MemberId memberId: digitMemberIds) {
    space.DeleteMember(memberId);
  }
  Ds::Vector<MemberId> filterMemberIds = space.Slice<Filter>();
  for (MemberId memberId: filterMemberIds) {
    space.DeleteMember(memberId);
  }
  Ds::Vector<MemberId> shifterMemberIds = space.Slice<Shifter>();
  for (MemberId memberId: shifterMemberIds) {
    space.DeleteMember(memberId);
  }
  InitializeLayers();
  nPlaceableIds.Clear();
}

void LevelSetup(size_t levelIdx) {
  MakeLevelEmpty();
  World::Space& space = World::nLayers.Back()->mSpace;
  const Level& level = nLevels[levelIdx];
  for (const Digit& digit: level.mDigits) {
    World::Object digitObject = space.CreateObject();
    digitObject.Add<Digit>() = digit;
    auto& transform = digitObject.Add<Comp::Transform>();
    Vec3 offset = {(float)digit.mCell[0], (float)digit.mCell[1], nDigitZ};
    transform.SetTranslation(nFieldOrigin + offset);
    transform.SetUniformScale(nDigitScale);
    auto& sprite = digitObject.Add<Comp::Sprite>();
    sprite.mMaterialId = "images:DigitBg";

    World::Object textChildObject = digitObject.CreateChild();
    auto& textTransform = textChildObject.Add<Comp::Transform>();
    textTransform.SetTranslation({0.0f, -0.25f, 0.1f});
    textTransform.SetUniformScale(0.5f);
    auto& text = textChildObject.Add<Comp::Text>();
    text.mColor = {1.0f, 1.0f, 1.0f, 1.0f};
    text.mAlign = Comp::Text::Alignment::Center;
    text.mText = std::to_string(digit.mValue);
  }

  for (const Filter& filter: level.mFilters) {
    World::Object filterObject = space.CreateObject();
    filterObject.Add<Filter>() = filter;
    auto& transform = filterObject.Add<Comp::Transform>();
    Vec3 offset = {
      (float)filter.mStartCell[0], (float)filter.mStartCell[1], nModifierZ};
    transform.SetTranslation(nFieldOrigin + offset);
    transform.SetUniformScale(nModifierScale);
    auto& sprite = filterObject.Add<Comp::Sprite>();
    sprite.mMaterialId = "images:ModifierBg";

    World::Object textChildObject = filterObject.CreateChild();
    auto& textTransform = textChildObject.Add<Comp::Transform>();
    textTransform.SetTranslation({0.0f, -0.25f, 0.1f});
    textTransform.SetUniformScale(0.5f);
    auto& text = textChildObject.Add<Comp::Text>();
    text.mColor = {0.0f, 0.0f, 0.0f, 1.0f};
    text.mAlign = Comp::Text::Alignment::Center;
    std::string filterChar = "";
    switch (filter.mType) {
    case Filter::Type::Add: filterChar = "+"; break;
    case Filter::Type::Sub: filterChar = "-"; break;
    case Filter::Type::Mul: filterChar = "*"; break;
    case Filter::Type::Mod: filterChar = "%"; break;
    }
    text.mText = filterChar + std::to_string(filter.mValue);

    if (filter.mPlaceable) {
      nPlaceableIds.Push(filterObject.mMemberId);
    }
  }

  for (const Shifter& shifter: level.mShifters) {
    World::Object shifterObject = space.CreateObject();
    shifterObject.Add<Shifter>() = shifter;
    auto& transform = shifterObject.Add<Comp::Transform>();
    Vec3 offset = {
      (float)shifter.mStartCell[0], (float)shifter.mStartCell[1], nModifierZ};
    transform.SetTranslation(nFieldOrigin + offset);
    transform.SetUniformScale(nModifierScale);
    auto& sprite = shifterObject.Add<Comp::Sprite>();
    sprite.mMaterialId = "images:ModifierBg";

    World::Object textChildObject = shifterObject.CreateChild();
    auto& textTransform = textChildObject.Add<Comp::Transform>();
    textTransform.SetTranslation({0.0f, -0.25f, 0.1f});
    textTransform.SetUniformScale(0.7f);
    switch (shifter.mDirection) {
    case Direction::Up:
      transform.SetRotation(Quat::AngleAxis(Math::nPiO2, {0.0f, 0.0f, 1.0f}));
      break;
    case Direction::Right: break;
    case Direction::Down:
      transform.SetRotation(Quat::AngleAxis(-Math::nPiO2, {0.0f, 0.0f, 1.0f}));
      break;
    case Direction::Left:
      transform.SetRotation(Quat::AngleAxis(Math::nPi, {0.0f, 0.0f, 1.0f}));
      break;
    }
    auto& text = textChildObject.Add<Comp::Text>();
    text.mColor = {0.0f, 0.0f, 0.0f, 1.0f};
    text.mAlign = Comp::Text::Alignment::Center;
    text.mText = ">";

    if (shifter.mPlaceable) {
      nPlaceableIds.Push(shifterObject.mMemberId);
    }
  }

  UpdatePlaceableGraphics();

  Ds::Vector<MemberId> digitIds = space.Slice<Digit>();
  for (MemberId memberId: digitIds) {
    auto& digit = space.Get<Digit>(memberId);
    nDigitLayer[digit.mCell[0]][digit.mCell[1]] = memberId;
  }
  Ds::Vector<MemberId> filterIds = space.Slice<Filter>();
  for (MemberId memberId: filterIds) {
    auto& filter = space.Get<Filter>(memberId);
    nModifierLayer[filter.mStartCell[0]][filter.mStartCell[1]] = memberId;
  }
  Ds::Vector<MemberId> shifterIds = space.Slice<Shifter>();
  for (MemberId memberId: shifterIds) {
    auto& shifter = space.Get<Shifter>(memberId);
    nModifierLayer[shifter.mStartCell[0]][shifter.mStartCell[1]] = memberId;
  }
}

void RegisterCustomTypes() {
  RegisterComponent(Digit);
  RegisterComponent(Filter);
  RegisterComponent(Shifter);
}

int main(int argc, char* argv[]) {
  Registrar::nRegisterCustomTypes = RegisterCustomTypes;
  Options::Config config;
  config.mWindowName = "Filtern";
  config.mProjectDirectory = PROJECT_DIRECTORY;
  config.mEditorLevel = Options::EditorLevel::Complete;
  Result result = VarkorInit(argc, argv, std::move(config));
  LogAbortIf(!result.Success(), result.mError.c_str());

  CreateLevels();
  InitializeLayers();
  FieldSetup();
  LevelSetup(0);
  World::nCentralUpdate = CentralUpdate;

  VarkorRun();
  VarkorPurge();
}
