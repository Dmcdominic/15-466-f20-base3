#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <stdexcept>
#include <iostream>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed, bool *quit) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;


	//----- SETTINGS -----
	const glm::vec3 NOTEBLOCK_ORIGIN = { -4.0f, -4.0f, 0.0f };
	const glm::vec2 NOTEBLOCK_DELTA  = {  2.0f,  2.0f };

	const int GRID_WIDTH = 5; // Number of different instruments
	const int GRID_HEIGHT = 5; // Number of different pitches


	//----- GAME STATE -----

	//time
	float prev_frame_time = 0.0f;
	float time = -1.0f;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	bool quit_pressed = false;

	bool freeplay = true; // TODO - add a toggle button for this? 'F'?
	bool showControls = true;

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
		std::vector<float> intervals = std::vector<float>(); // Pattern of seconds between each note
		float getFullInterval() {
			if (_fullInterval <= 0.0f) {
				_fullInterval = 0.0f;
				for (auto intervalIter = intervals.begin(); intervalIter != intervals.end(); intervalIter++) {
					_fullInterval += (*intervalIter);
				}
			}
			assert(_fullInterval > 0.0f);
			return _fullInterval;
		}
		private:
		float _fullInterval = 0.0f;
	};

	// vectors of shape and color definitions
	std::vector<ShapeDef> shapeDefs = std::vector<ShapeDef>({
		ShapeDef(SHAPE::CUBE,   "Cube",   { 0 }),
		ShapeDef(SHAPE::SPHERE, "Sphere", { 0, 1, 2 }),
		ShapeDef(SHAPE::CONE,   "Cone",   { 0, 0, 2, 2 }),
		ShapeDef(SHAPE::TORUS,  "Torus",  { 0, 2, 4, 1, 3, 0, 2, 4, 3, 1 }),
	});

	std::vector<ColorDef> colorDefs = std::vector<ColorDef>({
		ColorDef(COLOR::RED,   "Red",   { 1.0f }),
		ColorDef(COLOR::BLUE,  "Blue",  { 0.5f }),
		ColorDef(COLOR::GREEN, "Green", { 2.0f, 1.0f }),
		//ColorDef(COLOR::GREEN, "Green", { (2.0f / 3.0f) }),
		ColorDef(COLOR::PINK,  "Pink",  { 1.0f, 0.5f, 0.5f, 1.0f, 1.5f }),
	});

	// 2d vector of basic shapes/colors to duplicate
	std::vector<std::vector<Scene::Drawable*>> prefabs;

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

	// ===== NOTEBLOCKS =====

	class NoteBlock {
		public:
		ShapeDef *shapeDef = nullptr;
		ColorDef *colorDef = nullptr;
		Scene::Transform *transform = nullptr;
		glm::uvec2 gridPos = { 0, 0 }; // position in the grid. 0 <= x, y < 5
		std::shared_ptr< Sound::PlayingSample > currentSample = nullptr;
		//size_t last_tone_index = -1;
		//size_t last_interval_index = -1;
	};

	std::vector<std::vector<NoteBlock>> noteBlocks;

	void initNoteBlockVectors() {
		noteBlocks = std::vector<std::vector<NoteBlock>>(GRID_WIDTH);
		for (size_t i = 0; i < noteBlocks.size(); i++) {
			noteBlocks[i] = std::vector<NoteBlock>(GRID_HEIGHT, NoteBlock());
		}
	}


	// Drawable duplication based on Alyssa's game2 code, with permission:
	// https://github.com/lassyla/game2/blob/master/FishMode.cpp?fbclid=IwAR2gXxc_Omje47Xa7JmJPRN6Nh2jGSEnMVn1Qw7uoSV0QwKu0ZwwAUu5528
	NoteBlock* createNewNoteBlock(SHAPE s, COLOR c, glm::uvec2 gridPos) {
		Scene::Drawable *prefabDrawable = getPrefab(s, c);

		NoteBlock *nB = &noteBlocks[gridPos.x][gridPos.y];
		// TODO - check if there's already one here??
		if (nB->transform != nullptr) throw std::runtime_error("Tried to create new note block over an existing block");
		// Initialize NoteBlock values
		*nB = NoteBlock();
		nB->shapeDef = &(shapeDefs[int(s)]);
		nB->colorDef = &(colorDefs[int(c)]);
		nB->transform = new Scene::Transform();
		nB->transform->position = prefabDrawable->transform->position;
		nB->transform->rotation = prefabDrawable->transform->rotation;
		nB->transform->scale = prefabDrawable->transform->scale;
		nB->gridPos = gridPos;

		scene.drawables.emplace_back(nB->transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = prefabDrawable->pipeline;
		//noteBlocks.emplace_back(nB);
		return nB;
	}

	// Deletes a NoteBlock
	void deleteNoteBlock(NoteBlock* nB) {
		for (auto drawableIter = scene.drawables.begin(); drawableIter != scene.drawables.end(); drawableIter++) {
			if (drawableIter->transform == nB->transform) {
				scene.drawables.erase(drawableIter);
				for (auto nBColIter = noteBlocks.begin(); nBColIter != noteBlocks.end(); nBColIter++) {
					for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
						if (&(*nBIter) == nB) {
							if (nBIter->currentSample != nullptr) {
								nBIter->currentSample->stop(1.0f);
							}
							*nBIter = NoteBlock();
							return;
						}
					}
				}
				break;
			}
		}
		throw std::runtime_error("Tried to delete a NoteBlock but it wasn't found in the noteBlocks vector");
	}

	// Shifts all NoteBlocks by a certain amount in the grid
	void shiftNoteBlocks(int dx, int dy) {
		std::vector<std::vector<NoteBlock>> noteBlocksCpy(noteBlocks);
		initNoteBlockVectors();
		for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					size_t x = (int(nBIter->gridPos.x) + dx + GRID_WIDTH) % GRID_WIDTH;
					size_t y = (int(nBIter->gridPos.y) + dy + GRID_HEIGHT) % GRID_HEIGHT;
					noteBlocks.at(x).at(y) = *nBIter;
					noteBlocks.at(x).at(y).gridPos = glm::uvec2(x, y);
				}
			}
		}
	}

	// Cycles a NoteBlock's shape and/or color
	void cycleNoteBlock(NoteBlock &nB, int dShape, int dColor) {
		if (nB.transform == nullptr) return;
		SHAPE s = SHAPE((int(nB.shapeDef->shape) + dShape + shapeDefs.size()) % (shapeDefs.size()));
		COLOR c = COLOR((int(nB.colorDef->color) + dColor + colorDefs.size()) % (colorDefs.size()));
		deleteNoteBlock(&nB);
		createNewNoteBlock(s, c, glm::uvec2(0, 0));
	}

	// Updates the position of all NoteBlocks based on their gridPos
	void updateNoteBlockPositions() {
		for (auto nBColIter = noteBlocks.begin(); nBColIter != noteBlocks.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					nBIter->transform->position = NOTEBLOCK_ORIGIN +
						glm::vec3(nBIter->gridPos.x * NOTEBLOCK_DELTA.x, nBIter->gridPos.y * NOTEBLOCK_DELTA.y, 0.0f);
				}
			}
		}
	}

	//----- AUDIO UTIL -----
	size_t getTargetNote(float t, ColorDef *colorDef) {
		if (t < 0.0) return SIZE_MAX;
		float fullInterval = colorDef->getFullInterval();
		int targetInterval = std::max(0, int(time / fullInterval));
		float t_remaining = t - fullInterval * targetInterval;
		size_t targetNote = targetInterval * colorDef->intervals.size();
		for (auto intervalIter = colorDef->intervals.begin(); intervalIter != colorDef->intervals.end(); intervalIter++) {
			if (t_remaining <= 0.0f) {
				return targetNote;
			}
			t_remaining -= *intervalIter;
			targetNote++;
		}
		return targetNote;
	}

	void playNote(NoteBlock& nB, size_t targetNote);


	glm::vec3 get_left_speaker_position();
	glm::vec3 get_right_speaker_position();
	
	//camera:
	Scene::Camera *camera = nullptr;

};
