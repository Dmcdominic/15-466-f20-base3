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


	// ----- GAME STATE -----

	//time
	float prev_music_time = 0.0f;
	float music_time = -1.0f;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	bool quit_pressed = false;

	bool freeplay = false;
	bool showControls = true;

	bool playingTargetAudio = false;
	glm::vec3 question_mark_scale;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// Scene Transforms
	Scene::Transform *question_mark = nullptr;


	// ----- SHAPES AND COLORS -----

	enum class SHAPE {
		CUBE, /*SPHERE,*/ CONE, TORUS, END
	};
	enum class COLOR {
		RED, BLUE, GREEN, PINK, /*YELLOW, CYAN, WHITE, BLACK,*/ END
	};

	class ShapeDef {
		public:
		ShapeDef(SHAPE s, std::string n, std::vector<int> t_o, bool sb) : shape(s), name(n), tone_offsets(t_o), shiftblock(sb) {}
		SHAPE shape;
		std::string name;
		std::vector<int> tone_offsets = { 0 }; // Pattern of tones in the pentatonic scale to play, relative to the shape's row note
		bool shiftblock = false;
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
		ShapeDef(SHAPE::CUBE,   "Cube",   { 0 }, false),
		//ShapeDef(SHAPE::SPHERE, "Sphere", { 0 }),
		ShapeDef(SHAPE::CONE,   "Cone",   { 0 }, true),
		ShapeDef(SHAPE::TORUS,  "Torus",  { 0 }, true)
		/*ShapeDef(SHAPE::SPHERE, "Sphere", { 0, 1, 2 }),
		ShapeDef(SHAPE::CONE,   "Cone",   { 0, 0, 2, 2 }),
		ShapeDef(SHAPE::TORUS,  "Torus",  { 0, 2, 4, 1, 3, 0, 2, 4, 3, 1 }),*/
	});

	std::vector<ColorDef> colorDefs = std::vector<ColorDef>({
		ColorDef(COLOR::RED,   "Red",   { 1.0f }),
		ColorDef(COLOR::BLUE,  "Blue",  { 0.5f }),
		ColorDef(COLOR::GREEN, "Green", { 2.0f, 1.0f }),
		//ColorDef(COLOR::GREEN, "Green", { (2.0f / 3.0f) }),
		ColorDef(COLOR::PINK,  "Pink",  { 1.0f, 0.5f, 0.5f, 1.0f, 1.5f }), // TODO - make this interval actually random? (can be global pink random)
	});

	const float SHIFTBLOCK_T_OFFSET = 0.25f; // The time offset for when shiftblocks do their shift

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
	std::vector<std::vector<NoteBlock>> targetNoteBlocks;
	std::vector<std::vector<NoteBlock>> *editableNBs; // While iterating over noteBlocks and shifting notes, this is the vector that should actually be edited

	void initNoteBlockVectors(std::vector<std::vector<NoteBlock>> *nBs_to_init, bool fadeout = true) {
		// First check if any samples are playing that should be stopped
		for (auto nBColIter = nBs_to_init->begin(); nBColIter != nBs_to_init->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->currentSample != nullptr) {
					nBIter->currentSample->stop(fadeout ? 1.0f : 0.02f);
				}
			}
		}
		// Set empty vectors of correct length
		*nBs_to_init = std::vector<std::vector<NoteBlock>>(GRID_WIDTH);
		for (size_t i = 0; i < noteBlocks.size(); i++) {
			noteBlocks[i] = std::vector<NoteBlock>(GRID_HEIGHT, NoteBlock());
		}
	}


	// Drawable duplication based on Alyssa's game2 code, with permission:
	// https://github.com/lassyla/game2/blob/master/FishMode.cpp?fbclid=IwAR2gXxc_Omje47Xa7JmJPRN6Nh2jGSEnMVn1Qw7uoSV0QwKu0ZwwAUu5528
	NoteBlock* createNewNoteBlock(SHAPE s, COLOR c, glm::uvec2 gridPos) {
		std::cout << "createNewNoteBlock() called" << std::endl;
		Scene::Drawable *prefabDrawable = getPrefab(s, c);

		NoteBlock *nB = &noteBlocks[gridPos.x][gridPos.y];
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
		std::cout << "createNewNoteBlock() returning" << std::endl;
		return nB;
	}

	// Deletes a NoteBlock
	void deleteNoteBlock(NoteBlock* nB) {
		std::cout << "deleteNoteBlock() called" << std::endl;
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
							std::cout << "deleteNoteBlock() returning" << std::endl;
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
	void shiftNoteBlocks(int dx, int dy, int col = -1, int row = -1) {
		std::cout << "shiftNoteBlocks() called" << std::endl;
		std::vector<std::vector<NoteBlock>> noteBlocksCpy(noteBlocks);
		initNoteBlockVectors(&noteBlocks);
		size_t x, y, og_x = 0, og_y = 0;
		for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					// TODO - can shiftblocks themselves be shifted?? (have to have some "override" bool for WASD then)
					if ((col == -1 || col == nBIter->gridPos.x) &&
						  (row == -1 || row == nBIter->gridPos.y)) {
						x = (int(nBIter->gridPos.x) + dx + GRID_WIDTH) % GRID_WIDTH;
						y = (int(nBIter->gridPos.y) + dy + GRID_HEIGHT) % GRID_HEIGHT;
					} else {
						x = nBIter->gridPos.x;
						y = nBIter->gridPos.y;
					}
					std::cout << "setting (og_x, og_y) at editableNBs... (" << x << ", " << og_y << ")" << std::endl;
					std::cout << "noteBlocks.at(og_x).at(og_y).gridPos.y: " << (noteBlocks.at(og_x).at(og_y).gridPos.y) << std::endl;
					(*editableNBs).at(og_x).at(og_y) = *nBIter;
					(*editableNBs).at(og_x).at(og_y).gridPos = glm::uvec2(x, y);
				}
				og_y++;
			}
			og_x++;
		}
		std::cout << "shiftNoteBLocks() returning" << std::endl;
	}

	// Rotates all NoteBlocks in the 8 cells around the center
	// They progress one cell clockwise if CW == true, counter-clockwise otherwise
	void rotateNoteBlocks(glm::uvec2 center, bool CW) {
		std::vector<std::vector<NoteBlock>> noteBlocksCpy(noteBlocks);
		initNoteBlockVectors(&noteBlocks);
		int x, y;
		for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					// TODO - can shiftblocks themselves be shifted?? (have to have some "override" bool for WASD then)
					x = nBIter->gridPos.x;
					y = nBIter->gridPos.y;
					// TODO
					(*editableNBs).at(x).at(y) = *nBIter;
					(*editableNBs).at(x).at(y).gridPos = glm::uvec2(x, y);
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
					if (playingTargetAudio) {
						nBIter->transform->position = glm::vec3(100.0f, 100.0f, 100.0f);
					} else {
						nBIter->transform->position = NOTEBLOCK_ORIGIN +
							glm::vec3(nBIter->gridPos.x * NOTEBLOCK_DELTA.x, nBIter->gridPos.y * NOTEBLOCK_DELTA.y, 0.0f);
					}
				}
			}
		}
	}

	// Plays noteBlock samples, or shifts according to shiftblocks
	void updateNoteBlockSamples() {
		updateNoteBlockSamples(true);
		std::cout << "Y - noteBlocks.size(): " << noteBlocks.size() << std::endl;
		updateNoteBlockSamples(false);
	}
	void updateNoteBlockSamples(bool shiftblock_pass) {
		//assert(&editableNBs == &noteBlocks);
		std::vector<std::vector<NoteBlock>> noteBlocksCpy;
		if (shiftblock_pass) {
			editableNBs = &std::vector<std::vector<NoteBlock>>(noteBlocks);
			std::cout << "B - editableNBs->size(): " << editableNBs->size() << std::endl;
			initNoteBlockVectors(&noteBlocks);
			std::cout << "C - editableNBs->size(): " << editableNBs->size() << std::endl;
			std::cout << "C - noteBlocks.size(): " << noteBlocks.size() << std::endl;
		}
		// TODO
		for (auto nBColIter = noteBlocks.begin(); nBColIter != noteBlocks.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform == nullptr) continue;
				if (shiftblock_pass && !nBIter->shapeDef->shiftblock) continue;
				if (!shiftblock_pass && nBIter->shapeDef->shiftblock) continue;
				// Play a sample if necessary
				size_t prevFrameTargetNote = getTargetNote(prev_music_time, nBIter->colorDef, nBIter->shapeDef);
				size_t targetNote = getTargetNote(music_time, nBIter->colorDef, nBIter->shapeDef);

				/*std::cout << "prev_music_time: " << prev_music_time << std::endl;
				std::cout << "music_time: " << music_time << std::endl;
				std::cout << "prevFrameTargetNote: " << prevFrameTargetNote << std::endl;
				std::cout << "targetNote: " << targetNote << std::endl << std::endl;*/

				if (targetNote > prevFrameTargetNote || prevFrameTargetNote == SIZE_MAX) {
					std::cout << "Found a targetNote to play" << std::endl;
					playNote(*nBIter, targetNote);
				}

				// If it has a sample, update its position
				/*if (nBIter->currentSample != nullptr) {
					nBIter->currentSample->set_position(nBIter->transform->position, 0.0f);
				}*/
			}
		}
		std::cout << "W - noteBlocks.size(): " << noteBlocks.size() << std::endl;
		if (shiftblock_pass) {
			noteBlocks = std::vector<std::vector<NoteBlock>>(*editableNBs);
			editableNBs = &noteBlocks;
		}
		std::cout << "X - noteBlocks.size(): " << noteBlocks.size() << std::endl;
	}


	//----- AUDIO UTIL -----
	size_t getTargetNote(float t, ColorDef *colorDef, ShapeDef *shapeDef) {
		if (shapeDef->shiftblock) t += SHIFTBLOCK_T_OFFSET;
		if (t < 0.0) return SIZE_MAX;
		float fullInterval = colorDef->getFullInterval();
		int targetInterval = std::max(0, int(t / fullInterval));
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
