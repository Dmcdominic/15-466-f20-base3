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
	
	const float LVL_FINAL_LOOP_TIME = 5.0f;


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
	bool playingLvlFinalLoop = false;
	glm::vec3 question_mark_scale;
	glm::vec3 check_mark_scale;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// Scene Transforms
	Scene::Transform *question_mark = nullptr;
	Scene::Transform* check_mark = nullptr;


	// ----- SHAPES AND COLORS -----

	enum class SHAPE {
		CUBE, SPHERE, CONE, TORUS, END
	};
	enum class COLOR {
		RED, BLUE, GREEN, PINK, /*YELLOW, CYAN, WHITE, BLACK,*/ END
	};

	class ShapeDef {
		public:
		ShapeDef(SHAPE s, std::string n, std::vector<int> t_o, bool sb) : shape(s), name(n), tone_offsets(t_o) /*, shiftblock(sb)*/ {}
		SHAPE shape;
		std::string name;
		std::vector<int> tone_offsets = { 0 }; // Pattern of tones in the pentatonic scale to play, relative to the shape's row note
		//bool shiftblock = false;
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
		ShapeDef(SHAPE::SPHERE, "Sphere", { 0, 1, 2 }, true),
		ShapeDef(SHAPE::CONE,   "Cone",   { 0, 0, 2, 2 }, true),
		ShapeDef(SHAPE::TORUS,  "Torus",  { 0, 2, 4, 1, 3, 0, 2, 4, 3, 1 }, true),
	});

	std::vector<ColorDef> colorDefs = std::vector<ColorDef>({
		ColorDef(COLOR::RED,   "Red",   { 1.0f }),
		ColorDef(COLOR::BLUE,  "Blue",  { 0.5f }),
		ColorDef(COLOR::GREEN, "Green", { 0.25f, 0.25f, 1.0f }),
		//ColorDef(COLOR::GREEN, "Green", { (2.0f / 3.0f) }),
		ColorDef(COLOR::PINK,  "Pink",  { 2.0f, 1.0f, 0.5f, 0.25f, 0.25f }), // TODO - make this interval actually random? (can be global pink random)
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

	typedef std::vector<std::vector<NoteBlock>> nbVec;

	nbVec noteBlocks;
	nbVec targetNoteBlocks;
	//nbVec *editableNBs; // While iterating over noteBlocks and shifting notes, this is the vector that should actually be edited

	void initNoteBlockVectors(nbVec *nBs_to_init, bool fadeout = true) {
		// First check if any samples are playing that should be stopped
		for (auto nBColIter = nBs_to_init->begin(); nBColIter != nBs_to_init->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->currentSample) {
					nBIter->currentSample->stop(fadeout ? 1.0f : 0.02f);
				}
				/*if (nBIter->transform != nullptr) {
					deleteNoteBlock(nBs_to_init, &(*nBIter), (fadeout ? 1.0f : 0.02f));
				}*/
			}
		}
		// Set empty vectors of correct length
		*nBs_to_init = nbVec(GRID_WIDTH);
		for (size_t i = 0; i < nBs_to_init->size(); i++) {
			nBs_to_init->at(i) = std::vector<NoteBlock>(GRID_HEIGHT, NoteBlock());
		}
	}

	// Actually deletes all the noteBlocks before resetting the vector
	void clearNoteBlockVectors(nbVec* nBs_to_init, bool fadeout = true) {
		// First check if any samples are playing that should be stopped
		for (auto nBColIter = nBs_to_init->begin(); nBColIter != nBs_to_init->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					deleteNoteBlock(nBs_to_init, &(*nBIter), (fadeout ? 1.0f : 0.02f));
				}
			}
		}
		// Set empty vectors of correct length
		*nBs_to_init = nbVec(GRID_WIDTH);
		for (size_t i = 0; i < nBs_to_init->size(); i++) {
			nBs_to_init->at(i) = std::vector<NoteBlock>(GRID_HEIGHT, NoteBlock());
		}
	}


	// Drawable duplication based on Alyssa's game2 code, with permission:
	// https://github.com/lassyla/game2/blob/master/FishMode.cpp?fbclid=IwAR2gXxc_Omje47Xa7JmJPRN6Nh2jGSEnMVn1Qw7uoSV0QwKu0ZwwAUu5528
	NoteBlock* createNewNoteBlock(nbVec *nBs_to_create_in, SHAPE s, COLOR c, glm::uvec2 gridPos) {
		//std::cout << "createNewNoteBlock() called" << std::endl;
		Scene::Drawable *prefabDrawable = getPrefab(s, c);

		NoteBlock *nB = &(*nBs_to_create_in).at(gridPos.x).at(gridPos.y);
		if (nB->transform != nullptr) throw std::runtime_error("Tried to create new note block over an existing block");
		// Initialize NoteBlock values
		*nB = NoteBlock();
		nB->shapeDef = &(shapeDefs[int(s)]);
		nB->colorDef = &(colorDefs[int(c)]);
		nB->transform = new Scene::Transform();
		nB->transform->position = prefabDrawable->transform->position;
		nB->transform->rotation = prefabDrawable->transform->rotation;
		nB->transform->scale = prefabDrawable->transform->scale;
		if (nBs_to_create_in == &targetNoteBlocks) nB->transform->scale = glm::vec3(0.0f);
		nB->gridPos = gridPos;

		scene.drawables.emplace_back(nB->transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = prefabDrawable->pipeline;
		//std::cout << "createNewNoteBlock() returning" << std::endl;
		return nB;
	}

	// Deletes a NoteBlock
	void deleteNoteBlock(nbVec *noteblocks_to_delete_from, NoteBlock* nB, float fade = 1.0f) {
		for (auto drawableIter = scene.drawables.begin(); drawableIter != scene.drawables.end(); drawableIter++) {
			if (drawableIter->transform == nB->transform) {
				scene.drawables.erase(drawableIter);
				for (auto nBColIter = noteblocks_to_delete_from->begin(); nBColIter != noteblocks_to_delete_from->end(); nBColIter++) {
					for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
						if (&(*nBIter) == nB) {
							if (nBIter->currentSample != nullptr) {
								nBIter->currentSample->stop(fade);
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
	void shiftNoteBlocks(int dx, int dy, int col = -1, int row = -1) {
		nbVec noteBlocksCpy(noteBlocks);
		initNoteBlockVectors(&noteBlocks);
		int x, y;
		for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					// TODO - can shiftblocks themselves be shifted?? (have to have some "override" bool for WASD then)
					if ((col < 0 || (unsigned)col == nBIter->gridPos.x) &&
						  (row < 0 || (unsigned)row == nBIter->gridPos.y)) {
						x = (int(nBIter->gridPos.x) + dx + GRID_WIDTH) % GRID_WIDTH;
						y = (int(nBIter->gridPos.y) + dy + GRID_HEIGHT) % GRID_HEIGHT;
					} else {
						x = nBIter->gridPos.x;
						y = nBIter->gridPos.y;
					}
					noteBlocks.at(x).at(y) = *nBIter;
					noteBlocks.at(x).at(y).gridPos = glm::uvec2(x, y);
				}
			}
		}
	}

	// Rotates all NoteBlocks 90 degrees around the center (2, 2)
	void rotateFullGrid(bool clockwise) {
		nbVec noteBlocksCpy(noteBlocks);
		initNoteBlockVectors(&noteBlocks);
		int x, y;
		for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					if (clockwise) {
						y = (GRID_HEIGHT - 1) - nBIter->gridPos.x;
						x = nBIter->gridPos.y;
					} else {
						y = nBIter->gridPos.x;
						x = (GRID_WIDTH - 1) - nBIter->gridPos.y;
					}
					noteBlocks.at(x).at(y) = *nBIter;
					noteBlocks.at(x).at(y).gridPos = glm::uvec2(x, y);
				}
			}
		}
	}

	// Rotates all NoteBlocks in the 8 cells around the center
	// They progress one cell clockwise if CW == true, counter-clockwise otherwise
	//void rotateNoteBlocks(glm::uvec2 center, bool CW) {
	//	nbVec noteBlocksCpy(noteBlocks);
	//	initNoteBlockVectors(&noteBlocks);
	//	int x, y;
	//	for (auto nBColIter = noteBlocksCpy.begin(); nBColIter != noteBlocksCpy.end(); nBColIter++) {
	//		for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
	//			if (nBIter->transform != nullptr) {
	//				// TODO - can shiftblocks themselves be shifted?? (have to have some "override" bool for WASD then)
	//				x = nBIter->gridPos.x;
	//				y = nBIter->gridPos.y;
	//				// TODO
	//				(*editableNBs).at(x).at(y) = *nBIter;
	//				(*editableNBs).at(x).at(y).gridPos = glm::uvec2(x, y);
	//			}
	//		}
	//	}
	//}

	// Cycles a NoteBlock's shape and/or color
	void cycleNoteBlock(NoteBlock &nB, int dShape, int dColor) {
		if (nB.transform == nullptr) return;
		SHAPE s = SHAPE((int(nB.shapeDef->shape) + dShape + shapeDefs.size()) % (shapeDefs.size()));
		COLOR c = COLOR((int(nB.colorDef->color) + dColor + colorDefs.size()) % (colorDefs.size()));
		deleteNoteBlock(&noteBlocks, &nB);
		createNewNoteBlock(&noteBlocks, s, c, glm::uvec2(0, 0));
	}

	// Updates the position of all NoteBlocks based on their gridPos
	void updateNoteBlockPositions() {
		updateNoteBlockPositions(true);
		updateNoteBlockPositions(false);
	}
	void updateNoteBlockPositions(bool targetNBs) {
		nbVec nBs = (targetNBs ? targetNoteBlocks : noteBlocks);
		for (auto nBColIter = nBs.begin(); nBColIter != nBs.end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform != nullptr) {
					if (playingTargetAudio != targetNBs) {
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
	void updateNoteBlockSamples(nbVec* nBs_to_update) {
		//updateNoteBlockSamples(true);
		updateNoteBlockSamples(nBs_to_update, false);
	}
	void updateNoteBlockSamples(nbVec* nBs_to_update, bool shiftblock_pass) {
		//assert(&editableNBs == &noteBlocks);
		/*nbVec noteBlocksCpy;
		if (shiftblock_pass) {
			editableNBs = &nbVec(noteBlocks);
			initNoteBlockVectors(&noteBlocks);
		}*/
		for (auto nBColIter = nBs_to_update->begin(); nBColIter != nBs_to_update->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->transform == nullptr) continue;
				/*if (shiftblock_pass && !nBIter->shapeDef->shiftblock) continue;
				if (!shiftblock_pass && nBIter->shapeDef->shiftblock) continue;*/
				// Play a sample if necessary
				size_t prevFrameTargetNote = getTargetNote(prev_music_time, nBIter->colorDef, nBIter->shapeDef);
				size_t targetNote = getTargetNote(music_time, nBIter->colorDef, nBIter->shapeDef);

				/*std::cout << "prev_music_time: " << prev_music_time << std::endl;
				std::cout << "music_time: " << music_time << std::endl;
				std::cout << "prevFrameTargetNote: " << prevFrameTargetNote << std::endl;
				std::cout << "targetNote: " << targetNote << std::endl << std::endl;*/

				if (targetNote > prevFrameTargetNote /*|| prevFrameTargetNote == SIZE_MAX*/) {
					playNote(*nBIter, targetNote);
				}

				// If it has a sample, update its position
				/*if (nBIter->currentSample != nullptr) {
					nBIter->currentSample->set_position(nBIter->transform->position, 0.0f);
				}*/
			}
		}
		/*if (shiftblock_pass) {
			noteBlocks = nbVec(*editableNBs);
			editableNBs = &noteBlocks;
		}*/
	}


	//----- AUDIO UTIL -----
	size_t getTargetNote(float t, ColorDef *colorDef, ShapeDef *shapeDef) {
		//if (shapeDef->shiftblock) t += SHIFTBLOCK_T_OFFSET;
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

	// Stops the current sample of all noteBlocks in a 2D vector
	void stopAll(nbVec* nBs_to_update, float ramp = 0.0f) {
		for (auto nBColIter = nBs_to_update->begin(); nBColIter != nBs_to_update->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->currentSample != nullptr) {
					nBIter->currentSample->stop(ramp);
				}
			}
		}
	}

	// Sets the volume of all noteBlocks in a 2D vector
	void setVolume(nbVec* nBs_to_update, float volume, float ramp = 0.0f) {
		for (auto nBColIter = nBs_to_update->begin(); nBColIter != nBs_to_update->end(); nBColIter++) {
			for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
				if (nBIter->currentSample != nullptr) {
					nBIter->currentSample->set_volume(volume, ramp);
				}
			}
		}
	}


	// Level Management declarations
	const int TOTAL_LEVELS = 7;
	int current_lvl = 0;
	void startLevel(int lvl);
	bool doNoteBlocksMatchTarget();


	// Audio Util declarations
	void playNote(NoteBlock& nB, size_t targetNote);


	glm::vec3 get_left_speaker_position();
	glm::vec3 get_right_speaker_position();
	
	//camera:
	Scene::Camera *camera = nullptr;

};
