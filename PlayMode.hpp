#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed, bool *quit) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	bool quit_pressed = false;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	/*Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;*/

	enum class SHAPE {
		CUBE, SPHERE, /*CONE, TORUS,*/ END
	};
	enum class COLOR {
		RED, GREEN, BLUE, /*ORANGE, YELLOW, VIOLET, WHITE, BLACK,*/ END
	};

	// 2d vector of basic shapes/colors to duplicate
	std::vector<std::vector<Scene::Drawable>> prefabs;

	void initPrefabVectors() {
		prefabs = std::vector<std::vector<Scene::Drawable>>(int(SHAPE::END));
		for (size_t i = 0; i < prefabs.size(); i++) {
			prefabs[i] = std::vector<Scene::Drawable>(int(COLOR::END), NULL);
		}
	}

	Scene::Drawable getPrefab(SHAPE s, COLOR c) {
		return prefabs[int(s)][int(c)];
	}
	void setPrefab(SHAPE s, COLOR c, Scene::Drawable drawable) {
		prefabs[int(s)][int(c)] = drawable;
	}


	glm::vec3 get_left_speaker_position();
	glm::vec3 get_right_speaker_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
