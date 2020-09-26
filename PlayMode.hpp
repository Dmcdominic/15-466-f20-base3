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
		CUBE, SPHERE, CONE, TORUS, END
	};
	enum class COLOR {
		RED, BLUE, GREEN, PINK, /*YELLOW, CYAN, WHITE, BLACK,*/ END
	};

	class ShapeDef {
		public:
		ShapeDef(SHAPE s, std::string n, std::vector<int> t_o) : shape(s), name(n), tone_offsets(t_o) {}
		SHAPE shape;
		std::string name;
		std::vector<int> tone_offsets = { 0 }; // Pattern of tones in the pentatonic scale to play, relative to the shape's row note
	};

	class ColorDef {
		public:
		ColorDef(COLOR c, std::string n, std::vector<float> i) : color(c), name(n), intervals(i) {}
		COLOR color;
		std::string name;
		std::vector<float> intervals = { 1.0f }; // Pattern of seconds between each note
	};

	// vectors of shape and color definitions
	std::vector<ShapeDef> shapeDefs = std::vector<ShapeDef>({
		ShapeDef(SHAPE::CUBE,   "Cube",   { 0 }),
		ShapeDef(SHAPE::SPHERE, "Sphere", { 0, 1, 2 }),
		ShapeDef(SHAPE::CONE,   "Cone",   { 0, 0, 2, 2 }),
		ShapeDef(SHAPE::TORUS,  "Torus",  { 0, 2, 4, 1, 3, 0, 2, 4, 3, 1 }),
	});

	std::vector<ColorDef> colorDefs = std::vector<ColorDef>({
		ColorDef(COLOR::RED,   "Red",   { 0.5f }),
		ColorDef(COLOR::BLUE,  "Blue",  { 0.25f }),
		ColorDef(COLOR::GREEN, "Green", { (1.0f / 3.0f) }),
		ColorDef(COLOR::PINK,  "Pink",  { 0.5f, 0.25f, 0.25f, 0.5f, 0.75f }),
	});

	// 2d vector of basic shapes/colors to duplicate
	std::vector<std::vector<Scene::Drawable*>> prefabs;

	Scene::Drawable *prefab_cpy_test = nullptr;

	void initPrefabVectors() {
		prefabs = std::vector<std::vector<Scene::Drawable*>>(int(SHAPE::END));
		for (size_t i = 0; i < prefabs.size(); i++) {
			prefabs[i] = std::vector<Scene::Drawable*>(int(COLOR::END), nullptr);
		}
	}

	Scene::Drawable* getPrefab(SHAPE s, COLOR c) {
		return prefabs[int(s)][int(c)];
	}
	void setPrefab(SHAPE s, COLOR c, Scene::Drawable *drawable) {
		prefabs[int(s)][int(c)] = drawable;
	}

	// Note blocks
	class NoteBlock {
		public:
		ShapeDef *shapeDef;
		ColorDef *colorDef;
		Scene::Transform *transform;
		glm::uvec2 gridPos = { 0, 0 }; // position in the grid. 0 <= x, y < 5
	};

	std::vector<NoteBlock> noteBlocks = std::vector<NoteBlock>();

	// Drawable duplication based on Alyssa's game2 code, with permission:
	// https://github.com/lassyla/game2/blob/master/FishMode.cpp?fbclid=IwAR2gXxc_Omje47Xa7JmJPRN6Nh2jGSEnMVn1Qw7uoSV0QwKu0ZwwAUu5528
	Scene::Drawable* cpyPrefab(SHAPE s, COLOR c) {
		NoteBlock nB;
		nB.shapeDef = &(shapeDefs[int(s)]);
		nB.colorDef = &(colorDefs[int(c)]);
		nB.transform = new Scene::Transform();

		scene.drawables.emplace_back(nB.transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = getPrefab(s, c)->pipeline;
		noteBlocks.emplace_back(nB);
		return &drawable;
	}


	glm::vec3 get_left_speaker_position();
	glm::vec3 get_right_speaker_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
