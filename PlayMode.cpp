#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

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

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Audio/dusty-floor.opus"));
});

Load< std::vector<std::vector<Sound::Sample>> > PentaSamples(LoadTagDefault, []() -> std::vector<std::vector<Sound::Sample>> const* {
	return new std::vector< std::vector<Sound::Sample>>({
		std::vector <Sound::Sample> {
			Sound::Sample(data_path("Audio/AdVoca0.wav")),
			Sound::Sample(data_path("Audio/AdVoca1.wav")),
			Sound::Sample(data_path("Audio/AdVoca2.wav")),
			Sound::Sample(data_path("Audio/AdVoca3.wav")),
			Sound::Sample(data_path("Audio/AdVoca4.wav")),
		},
		// TODO - populate other instruments
	});
});

PlayMode::PlayMode() : scene(*pentaton_scene) {
	// Initialize prefab and NoteBlock vectors
	initPrefabVectors();
	initNoteBlockVectors();

	//get pointers to leg for convenience:
	/*for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;*/

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

	// TODO - delete this NoteBlock, for testing only
	//createNewNoteBlock(SHAPE::CUBE, COLOR::RED, glm::uvec2(0, 0));
	createNewNoteBlock(SHAPE::SPHERE, COLOR::GREEN, glm::uvec2(0, 0));

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
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
		} else if (evt.key.keysym.sym == SDLK_BACKSPACE ||
		           evt.key.keysym.sym == SDLK_BACKQUOTE) {
			quit_pressed = true;
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
	if (quit_pressed) {
		*quit = true;
		return;
	}

	if (time < 0.0f) {
		prev_frame_time = 0.0f - elapsed;
		time = 0.0f;
	} else {
		prev_frame_time = time;
		time += elapsed;
	}

	//slowly rotates through [0,1):
	/*wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);*/


	//float tenSinterval = fmod(time, 10.0f);
	//if (prefab_cpy_test == nullptr && tenSinterval < 5.0f) {
	//	leg_tip_loop = Sound::play_3D(AdVocaSamples->front(), 1.0f, get_right_speaker_position(), 10.0f);
	//	prefab_cpy_test = createNewNoteBlock(SHAPE::CUBE, COLOR::BLUE);
	//	//prefab_cpy_test->transform->position = glm::vec3(1.5f, 1.5f, 1.5f);
	//} else if (prefab_cpy_test != nullptr && tenSinterval > 5.0f) {
	//	leg_tip_loop->stop();
	//	deleteNoteBlock(prefab_cpy_test);
	//	prefab_cpy_test = nullptr;
	//}

	// Update NoteBlock positions
	updateNoteBlockPositions();

	// Update NoteBlock samples
	for (auto nBColIter = noteBlocks.begin(); nBColIter != noteBlocks.end(); nBColIter++) {
		for (auto nBIter = nBColIter->begin(); nBIter != nBColIter->end(); nBIter++) {
			if (nBIter->transform == nullptr) continue;
			// Play a sample if necessary
			float fullInterval = nBIter->colorDef->getFullInterval();
			size_t prevFrameTargetNote = getTargetNote(prev_frame_time, nBIter->colorDef);
			size_t targetNote = getTargetNote(time, nBIter->colorDef);
			std::cout << "prev_frame_time: " << prev_frame_time << std::endl;
			std::cout << "time: " << time << std::endl;
			std::cout << "prevFrameTargetNote: " << prevFrameTargetNote << std::endl;
			std::cout << "targetNote: " << targetNote << std::endl << std::endl;

			if (targetNote > prevFrameTargetNote || prevFrameTargetNote == SIZE_MAX) {
				playNote(*nBIter, targetNote);
			}

			// If it has a sample, update its position
			if (nBIter->currentSample != nullptr) {
				nBIter->currentSample->set_position(nBIter->transform->position, 0.0f);
			}
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
		lines.draw_text("escape ungrabs mouse; backspace or grave key to quit",
			glm::vec3(-aspect + 0.1f * H, -1.0f + 1.3f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("escape ungrabs mouse; backspace or grave key to quit",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 1.3f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));


		lines.draw_text("? to show/hide controls; more controls TBD",
			glm::vec3(-aspect + 0.1f * H, -1.0f + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("? to show/hide controls; more controls TBD",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0f + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}


// ===== AUDIO UTIL =====

void PlayMode::playNote(NoteBlock& nB, size_t targetNote) {
	std::cout << "targetNote: " << targetNote << std::endl;
	size_t targetTone = targetNote % nB.shapeDef->tone_offsets.size();
	std::cout << "targetTone: " << targetTone << std::endl;
	if (nB.currentSample != nullptr) {
		nB.currentSample->stop();
	}
	size_t instrument = nB.gridPos.x;
	size_t tone = (nB.gridPos.y + nB.shapeDef->tone_offsets[targetTone]) % GRID_HEIGHT;
	std::cout << "instrument: " << instrument << ". tone: " << tone << std::endl;
	nB.currentSample = Sound::play_3D(PentaSamples->at(instrument).at(tone), 1.0f, nB.transform->position, 10.0f);
}

glm::vec3 PlayMode::get_left_speaker_position() {
	return camera->transform->make_local_to_world()[3] + glm::vec3(-5.0f, 0.0f, 0.0f);
}

glm::vec3 PlayMode::get_right_speaker_position() {
	return camera->transform->make_local_to_world()[3] + glm::vec3(5.0f, 0.0f, 0.0f);
}