#!/usr/bin/env python

#####################################################################
# This script presents how to use the environment with PyOblige.
# https://github.com/mwydmuch/PyOblige
#####################################################################

from __future__ import print_function
from random import choice
from time import sleep
import matplotlib.pyplot as plt
import sys

from vizdoom import *
from oblige import *



DEFAULT_SEED = 666
generator = oblige.DoomLevelGenerator()
generator.set_seed(DEFAULT_SEED)
generator.set_config({"size": "micro", "health": "more", "weapons": "sooner"})
generator.set_config({"length": "episode"})
generator.generate("test_episode.wad")

game = DoomGame()

# Use your config
game.load_config("../../scenarios/oblige.cfg")
game.set_doom_map("map01")

for i in range(110):
    # Create Doom Level Generator instance and set optional seed.
    generator = DoomLevelGenerator()
    generator.set_seed(8945 + 9567*i)

    # Set generator configs, specified keys will be overwritten.
    generator.set_config({"size": "micro", "health": "more", "weapons": "sooner"})

    # There are few predefined sets of settings already defined in Oblige package, like test_wad and childs_play_wad
    #generator.set_config(test_wad)
    generator.set_config(monsters_play_wad)

    # Tell generator to generate few maps (options for "length": "single", "few", "episode", "game").
    generator.set_config({"length": "single"})

    # Generate method will return number of maps inside wad file.
    wad_path = "test2.wad"

    file_name = 'oblige_{:04d}.wad'.format(i)
    maps = generator.generate(file_name)

sys.exit()
# Set Scenario to the new generated WAD
game.set_doom_scenario_path("test2.wad")

# Sets up game for spectator (you)
game.add_game_args("+freelook 1")
game.set_screen_resolution(ScreenResolution.RES_640X480)
game.set_window_visible(True)
game.set_mode(Mode.SPECTATOR)

game.init()

# Play as many episodes as maps in the new generated WAD file.
episodes = maps

# Play until the game (episode) is over.
for i in range(1, episodes + 1):

    # Update map name
    print("Map {}/{}".format(i, episodes))
    map = "map{:02}".format(i)
    game.set_doom_map(map)
    game.new_episode()

    while not game.is_episode_finished():
        state = game.get_state()
        #labels = state.labels_buffer
        #plt.imsave("object.png", labels)
        #for label in state.labels:
        #    print(label.object_id, label.object_name)
        #sleep(0.1)

        game.advance_action()
        last_action = game.get_last_action()
        reward = game.get_last_reward()

        print("State #" + str(state.number))
        print("Game variables: ", state.game_variables)
        print("Action:", last_action)
        print("Reward:", reward)
        print("=====================")

    print("Episode finished!")
    print("Total reward:", game.get_total_reward())
    print("************************")
    sleep(2.0)

game.close()
