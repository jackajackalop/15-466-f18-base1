#include "CratesMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "WalkMesh.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>

using namespace std;

Load< MeshBuffer > crates_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("land.pnc"));
});

Load< WalkMesh > walk_mesh(LoadTagDefault, [](){
	return new WalkMesh(data_path("walk.blob"));
});

Load< GLuint > crates_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(crates_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< Sound::Sample > sample_dot(LoadTagDefault, [](){
	return new Sound::Sample(data_path("dot.wav"));
});
Load< Sound::Sample > sample_loop(LoadTagDefault, [](){
	return new Sound::Sample(data_path("loop.wav"));
});

CratesMode::CratesMode() {
	//----------------
	//set up scene:

//next chunk of code from Tiffany Li my light my savior the potato of my heart
	auto attach_object = [this](Scene::Transform *transform, std::string const &name) {
		Scene::Object *object = scene.new_object(transform);
		object->program = vertex_color_program->program;
		object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
		object->vao = *crates_meshes_for_vertex_color_program;
		MeshBuffer::Mesh const &mesh = crates_meshes->lookup(name);
		object->start = mesh.start;
		object->count = mesh.count;
		return object;
	};
        std::ifstream file("land.scene", std::ios::binary);

        std::vector< char > strings;
        read_chunk(file, "str0", &strings);

        struct TransformData {
            int parent_ref;
            uint32_t obj_name_begin, obj_name_end;
            float pos_x, pos_y, pos_z;
            float rot_x, rot_y, rot_z, rot_w;
            float scl_x, scl_y, scl_z;
        };

        std::vector< TransformData > transforms;
        read_chunk(file, "xfh0", &transforms);

	Scene::Transform *transform1 = scene.new_transform();
	transform1->position = glm::vec3(1.0f, 0.0f, 0.0f);
	player = attach_object(transform1, "Player");

        std::function< Scene::Transform *(int) > construct_transforms = 
		[&](int ref) -> Scene::Transform *{
            if (transform_dict.find(ref) != transform_dict.end())
                return transform_dict[ref];

            TransformData *transform = (TransformData *)(&transforms[ref]);
            Scene::Transform *new_transform = scene.new_transform();
            new_transform->position = glm::vec3(transform->pos_x, transform->pos_y, 
			    			transform->pos_z);
            new_transform->rotation = glm::quat(transform->rot_w, transform->rot_x, 
			    			transform->rot_y, transform->rot_z);
            new_transform->scale = glm::vec3(transform->scl_x, transform->scl_y, 
			    			transform->scl_z);
            new_transform->parent = transform->parent_ref < 0 ? 
		    nullptr : construct_transforms(transform->parent_ref);

            if (!(transform->obj_name_begin <= transform->obj_name_end 
				    && transform->obj_name_end <= strings.size())) {
                throw std::runtime_error(
				"object scene entry has out-of-range name begin/end");
            }

            transform_dict[ref] = new_transform;
            return new_transform;
        };

        struct MeshData {
        	int transform_ref;
        	uint32_t name_begin, name_end;
        };
        std::vector< MeshData > sceneObjects;
        read_chunk(file, "msh0", &sceneObjects);

        for (auto const &entry : sceneObjects) {
            if (!(entry.name_begin <= entry.name_end 
				    && entry.name_end <= strings.size())) {
                throw std::runtime_error(
				"mesh scene entry has out-of-range name begin/end");
            }
            std::string meshName(&strings[0] + entry.name_begin, 
			    &strings[0] + entry.name_end);

            if (!(entry.transform_ref >= 0 
				    && entry.transform_ref <= (int)transforms.size())) {
                throw std::runtime_error(
				"mesh scene entry has out of range transform ref");
            }

            Scene::Transform *transform_struct = 
		    construct_transforms(entry.transform_ref);
            Scene::Object *object = attach_object(transform_struct, meshName);
            if (meshName == "Player") {
                player = object;
		walk_point = walk_mesh->start(glm::vec3(1.0f, 0.0f, 0.0f));
		player->transform->position=walk_mesh->world_point(walk_point);
            }

        }


	{ //Camera looking at the origin:
		Scene::Transform *transform = scene.new_transform();
		transform->position = glm::vec3(0.0f, -20.0f, 10.0f);
		//Cameras look along -z, so rotate view to look at origin:
		transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		camera = scene.new_camera(transform);
	}
	
	//start the 'loop' sample playing at the large crate:
	loop = sample_loop->play(player->transform->position, 1.0f, Sound::Loop);
}

CratesMode::~CratesMode() {
	if (loop) loop->stop();
}

bool CratesMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//handle tracking the state of WSAD for movement control:
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_UP) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
			controls.left = (evt.type == SDL_KEYDOWN);
						return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
			controls.right = (evt.type == SDL_KEYDOWN);
						return true;
		}
	}
	return false;
}

void CratesMode::update(float elapsed) {
	float amt = 10.0f * elapsed;
	if (controls.right){
		player->transform->rotation = glm::normalize(
			player->transform->rotation
			* glm::angleAxis(-0.05f, glm::vec3(0.0f, 1.0f, 0.0f)));
	}
	if (controls.left){
		player->transform->rotation = glm::normalize(
			player->transform->rotation
			* glm::angleAxis(0.05f, glm::vec3(0.0f, 1.0f, 0.0f)));
	}
	glm::mat3 directions = glm::mat3_cast(player->transform->rotation);
	//TODO make player go in direction they are turned haha
	if (controls.backward){
//	       	player->transform->position += amt * directions[2];
		glm::vec3 step = directions[2] * amt;
		walk_mesh->walk(walk_point, step);
	}
	if (controls.forward){
	       	//player->transform->position -= amt * directions[2];		
		glm::vec3 step = directions[2] * -amt;
		walk_mesh->walk(walk_point, step);
	}
	player->transform->position = walk_mesh->world_point(walk_point);
	
	camera->transform->position = player->transform->position
		+ 100.0f/*20.0f*/ * directions[2]
		+ 10.0f * directions[1];
	camera->transform->rotation = player->transform->rotation;

	{ //set sound positions:
		glm::mat4 cam_to_world = camera->transform->make_local_to_world();
		Sound::listener.set_position( cam_to_world[3] );
		//camera looks down -z, so right is +x:
		Sound::listener.set_right( glm::normalize(cam_to_world[0]) );

	}

	dot_countdown -= elapsed;
	if (dot_countdown <= 0.0f) {
		dot_countdown = (rand() / float(RAND_MAX) * 2.0f) + 0.5f;
		glm::mat4x3 player_to_world = player->transform->make_local_to_world();
		sample_dot->play(player_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
	}
}

void CratesMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene.draw(camera);

	if (Mode::current.get() == this) {
		glDisable(GL_DEPTH_TEST);
		std::string message;
		if (mouse_captured) {
			message = "ESCAPE TO UNGRAB MOUSE * WASD MOVE";
		} else {
			message = "CLICK TO GRAB MOUSE * ESCAPE QUIT";
		}
		float height = 0.06f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,-0.99f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,-1.0f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);
	}

	GL_ERRORS();
}


void CratesMode::show_pause_menu() {
	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;

	menu->choices.emplace_back("PAUSED");
	menu->choices.emplace_back("RESUME", [game](){
		Mode::set_current(game);
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 1;

	Mode::set_current(menu);
}
