#pragma once
#include "scene3d.h"

namespace bambu::ui {
// Automatic idle rotation excludes printer events: those require real printer state.
inline Scene AutomaticScene(Emotion emotion,uint32_t idle_index,bool printer_state=false) {
    constexpr std::array<Scene,15> idle{{Scene::Game,Scene::Coffee,Scene::Music,Scene::Explore,
        Scene::Nap,Scene::Fly,Scene::Race,Scene::BubbleTea,Scene::Chill,Scene::Dance,
        Scene::Plant,Scene::PaperPlane,Scene::Magic,Scene::Stretch,Scene::Wave}};
    switch(emotion) {
        case Emotion::Printing: case Emotion::Warning:return printer_state?Scene::Printer:Scene::Game;
        case Emotion::Completed:return printer_state?Scene::Complete:Scene::Game;
        case Emotion::Error:return printer_state?Scene::Error:Scene::Game;
        case Emotion::Sleepy:return Scene::Nap;
        case Emotion::Idle:return idle[idle_index%idle.size()];
        default:return Scene::Game; // Clean head, without idle props during conversation.
    }
}
}
