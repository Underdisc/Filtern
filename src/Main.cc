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

bool nPaused = true;
float nAutomataTimePassed = 0.9f;
const Vec3 nFieldOrigin = {0.0f, 0.0f, 0.0f};
constexpr int nFieldWidth = 10;
constexpr int nFieldHeight = 10;
World::MemberId nDigitLayer[nFieldHeight][nFieldWidth];
World::MemberId nModifierLayer[nFieldHeight][nFieldWidth];

const float nFieldZ = 0.0f;
const float nModifierZ = 1.0f;
const float nDigitZ = 2.0f;

World::Object nRunDisplay;

struct Digit {
  int mCell[2];
  int mValue;
  Direction mDirection;
};

struct Filter {
  int mCell[2];
  int mValue;
  enum class Type { Add, Sub, Mul, Div };
  Type mType;
  bool mPlaceable;
};

struct Shifter {
  int mCell[2];
  Direction mDirection;
  bool mPlaceable;
};

struct Level {
  Ds::Vector<Digit> mDigits;
  Ds::Vector<Filter> mFilters;
  Ds::Vector<Shifter> mShifters;
};
Ds::Vector<Level> nLevels;
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
        .mCell = {5, 6},
        .mValue = 3,
        .mType = Filter::Type::Add,
        .mPlaceable = false,
      },
      {
        .mCell = {7, 4},
        .mValue = 1,
        .mType = Filter::Type::Sub,
        .mPlaceable = false,
      },
      {
        .mCell = {5, 2},
        .mValue = 3,
        .mType = Filter::Type::Mul,
        .mPlaceable = false,
      },
      {
        .mCell = {3, 4},
        .mValue = 3,
        .mType = Filter::Type::Div,
        .mPlaceable = false,
      },
    };
    nLevels.Emplace(std::move(level));
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
      case Filter::Type::Div:
        digit.mValue = digit.mValue / filter->mValue;
        break;
      }
      digit.mValue %= 10;
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

void RunPlaceMode() {}

void CentralUpdate() {
  if (Input::KeyPressed(Input::Key::Space)) {
    nPaused = !nPaused;
    auto& text = nRunDisplay.Get<Comp::Text>();
    if (nPaused) {
      text.mText = "=";
      nAutomataTimePassed = (float)(int)nAutomataTimePassed + 0.9f;
    }
    else {
      text.mText = ">";
    }
  }

  if (!nPaused) {
    RunAutomata();
  }
  else {
    RunPlaceMode();
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
  runDisplayText.mText = "=";

  World::Object camera = space.CreateObject();
  auto& cameraComp = camera.Add<Comp::Camera>();
  cameraComp.mProjectionType = Comp::Camera::ProjectionType::Orthographic;
  cameraComp.mHeight = 11.0f;
  layerIt->mCameraId = camera.mMemberId;
  auto& cameraTransform = camera.Get<Comp::Transform>();
  cameraTransform.SetTranslation({9.0f, 4.5f, 3.0f});
}

void LevelSetup(size_t levelIdx) {
  World::Space& space = World::nLayers.Back()->mSpace;

  const Level& level = nLevels[levelIdx];
  for (const Digit& digit: level.mDigits) {
    World::Object digitObject = space.CreateObject();
    digitObject.Add<Digit>() = digit;
    auto& transform = digitObject.Add<Comp::Transform>();
    Vec3 offset = {(float)digit.mCell[0], (float)digit.mCell[1], nDigitZ};
    transform.SetTranslation(nFieldOrigin + offset);
    transform.SetUniformScale(0.6f);
    auto& sprite = digitObject.Add<Comp::Sprite>();
    sprite.mMaterialId = "images:DigitBg";

    World::Object textChildObject = digitObject.CreateChild();
    auto& textTransform = textChildObject.Add<Comp::Transform>();
    textTransform.SetTranslation({0.0f, -0.25f, 0.1f});
    textTransform.SetUniformScale(0.5f);
    auto& text = textChildObject.Add<Comp::Text>();
    text.mColor = {0.0f, 0.0f, 0.0f, 1.0f};
    text.mAlign = Comp::Text::Alignment::Center;
    text.mText = std::to_string(digit.mValue);
  }

  for (const Filter& filter: level.mFilters) {
    World::Object filterObject = space.CreateObject();
    filterObject.Add<Filter>() = filter;
    auto& transform = filterObject.Add<Comp::Transform>();
    Vec3 offset = {(float)filter.mCell[0], (float)filter.mCell[1], nModifierZ};
    transform.SetTranslation(nFieldOrigin + offset);
    transform.SetUniformScale(0.7f);
    auto& sprite = filterObject.Add<Comp::Sprite>();
    sprite.mMaterialId = "images:DigitBg";

    World::Object textChildObject = filterObject.CreateChild();
    auto& textTransform = textChildObject.Add<Comp::Transform>();
    textTransform.SetTranslation({0.0f, -0.25f, 0.1f});
    textTransform.SetUniformScale(0.5f);
    auto& text = textChildObject.Add<Comp::Text>();
    text.mColor = {0.0f, 0.6f, 0.0f, 1.0f};
    text.mAlign = Comp::Text::Alignment::Center;
    std::string filterChar = "";
    switch (filter.mType) {
    case Filter::Type::Add: filterChar = "+"; break;
    case Filter::Type::Sub: filterChar = "-"; break;
    case Filter::Type::Mul: filterChar = "*"; break;
    case Filter::Type::Div: filterChar = "/"; break;
    }
    text.mText = filterChar + std::to_string(filter.mValue);
  }

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      nDigitLayer[j][i] = World::nInvalidMemberId;
      nModifierLayer[j][i] = World::nInvalidMemberId;
    }
  }

  Ds::Vector<MemberId> digitIds = space.Slice<Digit>();
  for (MemberId memberId: digitIds) {
    auto& digit = space.Get<Digit>(memberId);
    nDigitLayer[digit.mCell[0]][digit.mCell[1]] = memberId;
  }

  Ds::Vector<MemberId> filterIds = space.Slice<Filter>();
  for (MemberId memberId: filterIds) {
    auto& filter = space.Get<Filter>(memberId);
    nModifierLayer[filter.mCell[0]][filter.mCell[1]] = memberId;
  }
}

void RegisterCustomTypes() {
  RegisterComponent(Digit);
  RegisterComponent(Filter);
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
  FieldSetup();
  LevelSetup(0);
  World::nCentralUpdate = CentralUpdate;

  VarkorRun();
  VarkorPurge();
}
