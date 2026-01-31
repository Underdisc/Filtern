#include <Error.h>
#include <Temporal.h>
#include <VarkorMain.h>
#include <comp/BoxCollider.h>
#include <comp/Camera.h>
#include <comp/CameraOrbiter.h>
#include <comp/Sprite.h>
#include <comp/Transform.h>
#include <gfx/Renderer.h>
#include <imgui/imgui.h>
#include <math/Constants.h>
#include <world/Registrar.h>
#include <world/World.h>
#include <comp/Name.h>

void Setup() {
  Gfx::Renderer::nClearColor = {0.2f, 0.2f, 0.2f, 1.0};

  World::LayerIt layerIt = World::nLayers.EmplaceBack("Field");
  World::Space& space = layerIt->mSpace;

  World::Object field = space.CreateObject();
  auto& fieldTransform = field.Add<Comp::Transform>();
  fieldTransform.SetTranslation({0.0f, 0.0f, 0.0f});
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      World::Object gridSquare = field.CreateChild();
      auto& squareTransform = gridSquare.Add<Comp::Transform>();
      Vec3 startPosition = {-4.5f, -4.5f, 0.0f};
      Vec3 offset = {(float)j, (float)i, 0.0f};
      squareTransform.SetTranslation(startPosition + offset);
      squareTransform.SetUniformScale(0.8f);
      std::cout << squareTransform.GetTranslation() << std::endl;
      auto& squareSprite = gridSquare.Add<Comp::Sprite>();
      squareSprite.mMaterialId = "images:GridMaterial";
    }
  }

  World::Object camera = space.CreateObject();
  auto& cameraComp = camera.Add<Comp::Camera>();
  cameraComp.mProjectionType = Comp::Camera::ProjectionType::Orthographic;
  cameraComp.mHeight = 11.0f;
  layerIt->mCameraId = camera.mMemberId;
  auto& cameraTransform = camera.Get<Comp::Transform>();
  cameraTransform.SetTranslation({4.5f, 0.0f, 1.0f});
}

void RegisterCustomTypes() {
  using namespace Comp;
}

int main(int argc, char* argv[]) {
  Registrar::nRegisterCustomTypes = RegisterCustomTypes;
  Options::Config config;
  config.mWindowName = "Filtern";
  config.mProjectDirectory = PROJECT_DIRECTORY;
  config.mEditorLevel = Options::EditorLevel::Complete;
  Result result = VarkorInit(argc, argv, std::move(config));
  LogAbortIf(!result.Success(), result.mError.c_str());

  Setup();

  VarkorRun();
  VarkorPurge();
}
