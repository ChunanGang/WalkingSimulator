#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint phonebank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("city.pnct"));
	phonebank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > phonebank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("city.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = phonebank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = phonebank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > countDown(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("countdown.opus"));
});

Load< Sound::Sample > correctSE(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("correct.opus"));
});

Load< Sound::Sample > shotSE(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("shot.opus"));
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("city.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

PlayMode::PlayMode() : scene(*phonebank_scene) {

	for (auto &transform : scene.transforms) {
		//std::cout << "(" << transform.name <<  ")" << std::endl;
		if (transform.name == "Goal") goal = &transform;
	}
	goal->colorModifier = goalDefaultColor;
	auto mat = goal->make_local_to_world();
	goalWorldPos = glm::vec3(mat[3][0], mat[3][1], mat[3][2]);

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 0.7f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.transform->position = playerInitPos;
	player.transform->rotation = playerInitRot;
	player.at = walkmesh->nearest_walk_point(player.transform->position);

	// font
	hintFont = std::make_shared<TextRenderer>(data_path("OpenSans-B9K8.ttf"));
	messageFont = std::make_shared<TextRenderer>(data_path("SeratUltra-1GE24.ttf"));
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 up = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, up) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// check if win
	{
		static bool correctSEPlayed = false;
		if(win){
			if(!correctSEPlayed){
				Sound::play_3D(*correctSE, 0.4f, goalWorldPos);
				correctSEPlayed = true;
			}
			return;
		}
		// check
		auto mat = player.transform->make_local_to_world();
		glm::vec3 playerPos = glm::vec3(mat[3][0], mat[3][1], mat[3][2]);
		if (playerPos.y >= 9){
			win = true;
			gameOver = false;
			return;
		}
	}

	static bool shotPlayed = false;
	//check for gameover & restart
	if(gameOver){
		// SE
		if(!shotPlayed){
			Sound::play_3D(*shotSE, 0.2f, goalWorldPos);
			shotPlayed = true;
		}
		// restart
		if(space.pressed){
			gameOver = false;
			player.transform->position = playerInitPos;
			player.transform->rotation = playerInitRot;
			player.at = walkmesh->nearest_walk_point(player.transform->position);
			curVelocity = glm::vec3(0);
			canMove = false;
			startCountDown = false;
			shotPlayed = false;
			timeLeft = totalTime;
		}
		left.pressed = false;
		right.pressed = false;
		up.pressed = false;
		down.pressed = false;
	}

	//player walking:
	{
		//combine inputs into a move:
		glm::vec2 force = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) force.x =-1.0f;
		if (!left.pressed && right.pressed) force.x = 1.0f;
		if (down.pressed && !up.pressed) force.y =-1.0f;
		if (!down.pressed && up.pressed) force.y = 1.0f;

		// update velocity
		glm::vec2 newVelocity = glm::vec3(0);
		// apllying force (has input)
		if (force != glm::vec2(0.0f)) {
			force = glm::normalize(force);
			// new velicoty
			newVelocity = curVelocity + force * acceleration * elapsed;
		}
		// no force, use firction
		else{
			if (curVelocity != glm::vec2(0)){
				newVelocity = curVelocity - glm::normalize(curVelocity) * friction * elapsed;
				// make sure frictiuon does not change the direction
				if ((curVelocity.x>0 && newVelocity.x <0) || (curVelocity.x<0 && newVelocity.x >0))
					newVelocity.x = 0;
				if ((curVelocity.y>0 && newVelocity.y <0) || (curVelocity.y<0 && newVelocity.y >0)) 
					newVelocity.y = 0;
			}
		}
		// movement
		glm::vec2 move = (newVelocity + curVelocity) / 2.0f * elapsed;
		curVelocity = newVelocity;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);
				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		// check if play moving when can't
		if(!canMove && move != glm::vec2(0))
			gameOver = true;
		// time up
		if(timeLeft <= 0)
			gameOver = true;
	}

	if (gameOver)
		return;

	goal->colorModifier = goalDefaultColor;
	// play sound
	{
		static float soundWaitCount = 0;
		soundWaitCount += elapsed;
		if(soundWaitCount >= soundWaitTime){
			soundWaitCount = 0;
			Sound::play_3D(*countDown, 1, goalWorldPos);
			// start count down when sound starts to play
			startCountDown = true;
			canMove = true;
		}
	}
	// count down
	if(startCountDown){
		static float countDownCount = 0;
		countDownCount += elapsed;
		if(countDownCount >= countDownTime){
			countDownCount = 0;
			canMove = false;
			startCountDown = false;
		}
		// gola color
		goalBlinkModifier += elapsed ;
		goalBlinkModifier -= std::floor(goalBlinkModifier);
		goal->colorModifier = glm::vec3(
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (goalBlinkModifier + 0.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (goalBlinkModifier + 1.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (goalBlinkModifier + 2.0f / 3.0f) ) ) ))) /255.0f
		);
	}

	timeLeft -= elapsed;
	if (timeLeft < 0) timeLeft = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	if(gameOver){
		if(timeLeft<=0)
			messageFont->draw("TIME'S UP. YOU DIE", 10.0f, 300.0f, glm::vec2(0.8,0.8), glm::vec3(0.7f, 0.2f, 0.4f));
		else
			messageFont->draw("YOU DIE", 200.0f, 300.0f, glm::vec2(0.8,0.8), glm::vec3(0.7f, 0.2f, 0.4f));
		messageFont->draw("Press Space to Restart", 150.0f, 200.0f, glm::vec2(0.25,0.25), glm::vec3(0.9f, 0.4f, 0.7f));
	}
	if(win)
		messageFont->draw("VICTORY", 200.0f, 300.0f, glm::vec2(0.8,0.8), glm::vec3(0.4f, 0.5f, 0.9f));
	else{
		if(canMove){
			hintFont->draw("Move Now", 20.0f, 550.0f, glm::vec2(0.2,0.25), glm::vec3(0.2, 0.8f, 0.2f));
		}
		else{
			hintFont->draw("STOP !!!", 20.0f, 550.0f, glm::vec2(0.2,0.25), glm::vec3(1.0f, 0.2f, 0.2f));
		}
		hintFont->draw("Time Left: " + std::to_string((int)(timeLeft+0.9)), 650.0f, 550.0f, glm::vec2(0.2,0.25), glm::vec3(.6f, 0.9f, 0.4f));
	}

	GL_ERRORS();
}
