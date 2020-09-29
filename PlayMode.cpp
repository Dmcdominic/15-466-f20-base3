#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <time.h>

GLuint pentaton_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > pentaton_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("pentaton.pnct"));
	pentaton_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > pentaton_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("pentaton.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = pentaton_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = pentaton_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});


// Load samples!
Load< std::vector<std::vector<Sound::Sample>> > PentaSamples(LoadTagDefault, []() -> std::vector<std::vector<Sound::Sample>> const* {
	return new std::vector< std::vector<Sound::Sample>>({
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/PianoFB/F3.wav")),
			Sound::Sample(data_path("Audio/PianoFB/G3.wav")),
			Sound::Sample(data_path("Audio/PianoFB/A3.wav")),
			Sound::Sample(data_path("Audio/PianoFB/C4.wav")),
			Sound::Sample(data_path("Audio/PianoFB/D4.wav")),
		},
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/PickedBassYR/F.wav")),
			Sound::Sample(data_path("Audio/PickedBassYR/G.wav")),
			Sound::Sample(data_path("Audio/PickedBassYR/A.wav")),
			Sound::Sample(data_path("Audio/PickedBassYR/C.wav")),
			Sound::Sample(data_path("Audio/PickedBassYR/D.wav")),
		},
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/ColomboADK/BassDrum-HV1.wav")),
			Sound::Sample(data_path("Audio/ColomboADK/ClosedHiHat-1.wav")),
			Sound::Sample(data_path("Audio/ColomboADK/OpenHiHat-1.wav")),
			Sound::Sample(data_path("Audio/ColomboADK/SnareDrum1-HV1.wav")),
			Sound::Sample(data_path("Audio/ColomboADK/SideStick-1.wav")),
		},
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/SpanishClassicalGuitar/F3.wav")),
			Sound::Sample(data_path("Audio/SpanishClassicalGuitar/G3.wav")),
			Sound::Sample(data_path("Audio/SpanishClassicalGuitar/A3.wav")),
			Sound::Sample(data_path("Audio/SpanishClassicalGuitar/C4.wav")),
			Sound::Sample(data_path("Audio/SpanishClassicalGuitar/D4.wav")),
		},
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/AdVoca/F.wav")),
			Sound::Sample(data_path("Audio/AdVoca/G.wav")),
			Sound::Sample(data_path("Audio/AdVoca/A.wav")),
			Sound::Sample(data_path("Audio/AdVoca/C.wav")),
			Sound::Sample(data_path("Audio/AdVoca/D.wav")),
		},
	});
});


PlayMode::PlayMode() : scene(*pentaton_scene) {
	// First, seed the random number generator
	std::srand((unsigned int)time(NULL));

	// Initialize prefab and NoteBlock vectors
	initPrefabVectors();
	/*initNoteBlockVectors(&noteBlocks);
	initNoteBlockVectors(&targetNoteBlocks);*/
	//editableNBs = &noteBlocks;

	//get pointers to scene objects
	for (auto &transform : scene.transforms) {
		if (transform.name == "Question Mark") question_mark = &transform;
		if (transform.name == "Check Mark") check_mark = &transform;
	}
	if (question_mark == nullptr) throw std::runtime_error("question_mark not found.");
	if (check_mark == nullptr) throw std::runtime_error("check_mark not found.");

	// Get pointers to prefabs
	for (auto &drawable : scene.drawables) {
		for (ShapeDef shapeDef : shapeDefs) {
			for (ColorDef colorDef : colorDefs) {
				if (drawable.transform->name == shapeDef.name + colorDef.name) {
					setPrefab(shapeDef.shape, colorDef.color, &drawable);
				}
			}
		}
	}
	// Check if vectors are null
	for (size_t i = 0; i < prefabs.size(); i++) {
		for (size_t j = 0; j < prefabs[i].size(); j++) {
			if (prefabs[i][j] == nullptr) {
				std::cout << "prefab " << i << ", " << j << " not found" << std::endl;
				throw std::runtime_error("prefab not found");
			}
		}
	}

	// Init some standard values
	question_mark_scale = question_mark->scale;
	check_mark_scale = check_mark->scale;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	// start level 0
	startLevel(0);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {  // Free the mouse
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_BACKSPACE ||
			evt.key.keysym.sym == SDLK_BACKQUOTE) { // QUIT
			quit_pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SLASH) { // Toggle showControls
			showControls = !showControls;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) { // Toggle freeplay
			freeplay = !freeplay;
			playingTargetAudio = false;
			playingLvlFinalLoop = false;
			if (!freeplay) {
				startLevel(current_lvl);
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) { // Hold for target audio (level), Construct/destruct (freeplay)
			space.downs += 1;
			space.pressed = true;
			if (freeplay) {
				NoteBlock* nB = &(noteBlocks.at(0).at(0));
				if (nB->transform != nullptr) {
					deleteNoteBlock(&noteBlocks, nB);
				}
				else {
					createNewNoteBlock(&noteBlocks, SHAPE::CUBE, COLOR::RED, glm::uvec2(0, 0));
				}
			}
			return true;
		}
		// All keys checked below will NOT be interactable during the lvlFinalLoop or while playingTargetAudio
		if (playingLvlFinalLoop || playingTargetAudio) return false;
		// All keys checked below will NOT be interactable during the lvlFinalLoop or while playingTargetAudio
		if (evt.key.keysym.sym == SDLK_a) { // Shift left
			left.downs += 1;
			left.pressed = true;
			if (!playingTargetAudio) shiftNoteBlocks(-1, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) { // Shift right
			right.downs += 1;
			right.pressed = true;
			if (!playingTargetAudio) shiftNoteBlocks(1, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) { // Shift up
			up.downs += 1;
			up.pressed = true;
			if (!playingTargetAudio) shiftNoteBlocks(0, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) { // Shift down
			down.downs += 1;
			down.pressed = true;
			if (!playingTargetAudio) shiftNoteBlocks(0, -1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_e) { // Rotate clockwise
			rotateFullGrid(true);
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) { // Rotate counter-clockwise
			rotateFullGrid(false);
			return true;
		} else if (evt.key.keysym.sym == SDLK_LEFT) {             // Cycle (freeplay)
			if (freeplay) cycleNoteBlock(noteBlocks[0][0], -1, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {            // Cycle (freeplay)
			if (freeplay) cycleNoteBlock(noteBlocks[0][0], 1, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {               // Cycle (freeplay)
			if (freeplay) cycleNoteBlock(noteBlocks[0][0], 0, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {             // Cycle (freeplay)
			if (freeplay) cycleNoteBlock(noteBlocks[0][0], 0, -1);
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
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
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
			// Uncomment for mouse movement to control the camera
			/*glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);*/
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed, bool *quit) {
	// Check user input (quit)
	if (quit_pressed) {
		*quit = true;
		return;
	}

	// Check user input (space for target audio)
	if (!freeplay && !playingLvlFinalLoop) {
		if (space.pressed && !playingTargetAudio) {
			playingTargetAudio = true;
			music_time = -0.1f;
			stopAll(&noteBlocks);
		} else if (!space.pressed && playingTargetAudio) {
			playingTargetAudio = false;
			music_time = -0.1f;
			stopAll(&targetNoteBlocks);
		}
	}

	/*std::cout << "editableNBs.size(): " << editableNBs.size() << std::endl;
	if (editableNBs.size() > 0 && editableNBs.at(0).size() > 0) {
		std::cout << "color at (0, 0): " << int(editableNBs.at(0).at(0).colorDef->color) << std::endl;
	}*/

	// Update time
	/*std::cout << "prev_music_time: " << prev_music_time << std::endl;
	std::cout << "music_time: " << music_time << std::endl;*/
	if (music_time < 0.0f) {
		prev_music_time = 0.0f - std::max(0.001f, elapsed);
		music_time = 0.0f;
	} else {
		prev_music_time = music_time;
		music_time += elapsed;
	}
	/*std::cout << "NEW prev_music_time: " << prev_music_time << std::endl;
	std::cout << "NEW music_time: " << music_time << std::endl;*/


	// Update based on playingTargetAudio
	if (playingTargetAudio) {
		question_mark->scale = question_mark_scale;
	} else {
		question_mark->scale = glm::vec3(0.0f);
	}

	if (playingLvlFinalLoop) {
		check_mark->scale = check_mark_scale;
	} else {
		check_mark->scale = glm::vec3(0.0f);
	}


	// Update NoteBlock positions and samples
	updateNoteBlockPositions();
	if (playingTargetAudio) {
		updateNoteBlockSamples(&targetNoteBlocks);
	} else {
		updateNoteBlockSamples(&noteBlocks);
	}


	if (!freeplay) {
		// Check if noteBlocks match targetNoteBlocks and you've completed the level
		if (playingLvlFinalLoop &&
		    (music_time > LVL_FINAL_LOOP_TIME ||
				(music_time > 1.1f && space.pressed))) {
			startLevel((current_lvl + 1) % TOTAL_LEVELS);
		}

		if (!playingLvlFinalLoop && doNoteBlocksMatchTarget()) {
			playingLvlFinalLoop = true;
			playingTargetAudio = false;
			music_time = -0.1f;
			stopAll(&targetNoteBlocks);
		}
	}


	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 at = frame[3];
		Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		float ofs = 2.0f / drawable_size.y;

		// Level/freeplay text
		std::string str = "Level " + std::to_string(current_lvl + 1);
		if (playingLvlFinalLoop) str += " complete!";
		if (freeplay) str = "Freeplay";
		lines.draw_text(str,
			glm::vec3(-aspect + 0.1f * H, 1.0f - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text(str,
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0f - 1.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		// Controls text
		if (showControls) {
			if (freeplay) {
				lines.draw_text("Arrow keys to cycle the origin shape/color",
					glm::vec3(-aspect + 0.1f * H, -1.0f + 4.9f * H, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0x00, 0x00, 0x00, 0x00));
				lines.draw_text("Arrow keys to cycle the origin shape/color",
					glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 4.9f * H + ofs, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}

			lines.draw_text("WASD to shift;  Q/E to rotate",
				glm::vec3(-aspect + 0.1f * H, -1.0f + 3.7f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("WASD to shift;  Q/E to rotate",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 3.7f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			if (!freeplay) {
				lines.draw_text("Hold space to hear the goal",
					glm::vec3(-aspect + 0.1f * H, -1.0f + 2.5f * H, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0x00, 0x00, 0x00, 0x00));
				lines.draw_text("Hold space to hear the goal",
					glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 2.5f * H + ofs, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			} else {
				lines.draw_text("Space to create/destroy at origin",
					glm::vec3(-aspect + 0.1f * H, -1.0f + 2.5f * H, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0x00, 0x00, 0x00, 0x00));
				lines.draw_text("Space to create/destroy at origin",
					glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 2.5f * H + ofs, 0.0),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}

			lines.draw_text("enter to toggle freeplay;  ? to show/hide controls",
				glm::vec3(-aspect + 0.1f * H, -1.0f + 1.3f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("enter to toggle freeplay;  ? to show/hide controls",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 1.3f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("escape ungrabs mouse;  backspace to quit",
				glm::vec3(-aspect + 0.1f * H, -1.0f + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("escape ungrabs mouse;  backspace to quit",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 0.1f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
}


// ===== LEVEL MANAGEMENT =====
void PlayMode::startLevel(int lvl) {
	current_lvl = lvl;
	playingLvlFinalLoop = false;
	playingTargetAudio = false;
	freeplay = false;

	clearNoteBlockVectors(&noteBlocks);
	clearNoteBlockVectors(&targetNoteBlocks);
	for (int i = 0; i < 2; i++) {
		nbVec *nBs = (i == 0) ? &noteBlocks : &targetNoteBlocks;
		if (lvl == 0) {
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(3, 3));
		} else if (lvl == 1) {
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(1, 0));
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::BLUE, glm::uvec2(0, 3));
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(2, 0));
		} else if (lvl == 2) {
			createNewNoteBlock(nBs, SHAPE::SPHERE, COLOR::RED, glm::uvec2(2, 0));
		} else if (lvl == 3) {
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(2, 0));
			createNewNoteBlock(nBs, SHAPE::SPHERE, COLOR::BLUE, glm::uvec2(4, 1));
		} else if (lvl == 4) {
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::PINK, glm::uvec2(2, 0));
			createNewNoteBlock(nBs, SHAPE::SPHERE, COLOR::GREEN, glm::uvec2(1, 1));
		} else if (lvl == 5) {
			createNewNoteBlock(nBs, SHAPE::CONE, COLOR::RED, glm::uvec2(2, 0));
			createNewNoteBlock(nBs, SHAPE::TORUS, COLOR::BLUE, glm::uvec2(3, 2));
		} else if (lvl == 6) {
			createNewNoteBlock(nBs, SHAPE::SPHERE, COLOR::GREEN, glm::uvec2(0, 2));
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(1, 0));
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::BLUE, glm::uvec2(2, 0));
			createNewNoteBlock(nBs, SHAPE::TORUS, COLOR::BLUE, glm::uvec2(2, 1));
			createNewNoteBlock(nBs, SHAPE::CONE, COLOR::PINK, glm::uvec2(3, 3));
			createNewNoteBlock(nBs, SHAPE::CUBE, COLOR::RED, glm::uvec2(4, 0));
		}
	}
	do {
		// Shift noteBlocks a random (non-zero) amount
		int dx = (rand() % (GRID_WIDTH - 1)) + 1;
		int dy = (rand() % (GRID_HEIGHT - 1)) + 1;
		shiftNoteBlocks(dx, dy);
		// Rotate randomly, either clockwise or counter-clockwise
		rotateFullGrid(rand() % 2 == 0);
	} while (doNoteBlocksMatchTarget());
}

// Returns true iff noteBlocks matches targetNoteBlocks
bool PlayMode::doNoteBlocksMatchTarget() {
	for (auto nBColIter = targetNoteBlocks.begin(); nBColIter != targetNoteBlocks.end(); nBColIter++) {
		for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
			if (nBIter->transform != nullptr) {
				NoteBlock nb = noteBlocks.at(nBIter->gridPos.x).at(nBIter->gridPos.y);
				if (nb.transform == nullptr) return false;
				if (nb.colorDef != nBIter->colorDef) return false;
				if (nb.shapeDef != nBIter->shapeDef) return false;
			}
		}
	}
	return true;
}


// ===== AUDIO UTIL =====

// Standard CUBE shape actually plays a note
// Other shapes move other NoteBlocks
void PlayMode::playNote(NoteBlock& nB, size_t targetNote) {
	//if (nB.shapeDef->shape == SHAPE::CUBE) { // CUBE actually plays a note
		//std::cout << "targetNote: " << targetNote << std::endl;
		size_t targetTone = targetNote % nB.shapeDef->tone_offsets.size();
		//std::cout << "targetTone: " << targetTone << std::endl;
		if (nB.currentSample != nullptr) {
			nB.currentSample->stop();
		}
		size_t instrument = nB.gridPos.x;
		size_t tone = (nB.gridPos.y + nB.shapeDef->tone_offsets[targetTone]) % GRID_HEIGHT;
		//std::cout << "instrument: " << instrument << ". tone: " << tone << std::endl;
		nB.currentSample = Sound::play_3D(PentaSamples->at(instrument).at(tone), 1.0f, nB.transform->position, 10.0f);
	//} else if (nB.shapeDef->shape == SHAPE::CONE) { // CONE shifts its column upward
	//	shiftNoteBlocks(0, 1, nB.gridPos.x, -1);
	//} else if (nB.shapeDef->shape == SHAPE::TORUS) { // TORUS rotates the blocks around it
	//	rotateNoteBlocks(nB.gridPos, true);
	//}
}

glm::vec3 PlayMode::get_left_speaker_position() {
	return camera->transform->make_local_to_world()[3] + glm::vec3(-5.0f, 0.0f, 0.0f);
}

glm::vec3 PlayMode::get_right_speaker_position() {
	return camera->transform->make_local_to_world()[3] + glm::vec3(5.0f, 0.0f, 0.0f);
}