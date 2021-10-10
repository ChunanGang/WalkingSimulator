#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>
#include "TextRenderer.hpp"

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	Scene::Transform * goal;
	glm::vec3 goalWorldPos;

	// game logics
	bool win = false;
	bool gameOver = false;
	const glm::vec3 playerInitPos = glm::vec3(-7,-1,0);
	const glm::quat playerInitRot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::vec2 curVelocity = glm::vec3(0);
	float acceleration = 1.5f;
	float friction = 1.0f;
	const float soundWaitTime = 6.0f;
	const float countDownTime = 3.0f;
	const float totalTime = 43.0f;
	float timeLeft = totalTime;
	//
	bool canMove = false;
	bool startCountDown = false;
	// color
	float goalBlinkModifier;
	glm::vec3 goalDefaultColor = glm::vec3(0.1f,1,0.1f);

	std::shared_ptr<TextRenderer> hintFont;
	std::shared_ptr<TextRenderer> messageFont;
};
