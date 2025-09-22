# Monitor your favorite roblox game's ccu! (aka watch how gag ccu drops in X minutes)
Hello! Welcome to the README.md 
This programm is a monitoring tool for windows (for now, linux support isnt intended yet + im not sure how many linux users play roblox) to monitor ccu and rating rates.
This is complete shitcode made by my ideas, [GitHub Copilot](https://github.com/features/copilot), [Claude](https://claude.ai/), and [ChatGPT](https://chatgpt.com/). 
The code is currently quite buggy with output and I will abuse AIs more to fix them allthough, your ideas and pull requests are welcomed aswell.

# Why this exists?
Simple, [RoMonitor](https://romonitorstats.com/) or the Roblox Page sometimes in peak hours have loading issues,
and i wasn't that sleepy at a certian night so yeah, here we are.

# How to get the programm?
There are 2 ways:
1. Clone the repo and build it yourself (it has a cool way to build in VSCode but you need g++)
    1.1. Open root folder of the file in VSCode
    1.2. Click on the search tab on the top of the UI
    1.3. Select Run Task
    1.4. Select Build Roblox Monitor
    1.5. Build should appear in the build folder
2. Get the .exe if i'll understand how to make release notes

# How to use this programm?
Here is a step-by-step
1. In the start of the programm you will be asked to use comparison mode or not. Write y for yes or, n for no
2. Input the universeID(s (incase you chose comparison mode)) Read below about it (or maybe ill do a FAQ section if this gets enough questions or users)
3. Input the amount of minutes to monitor for
4. Wait that amount of minutes and you will get your logs! 
5. give this repo a star

# Where do I get a UniverseID? (Future FAQ section)
1. Go to your game's front page on your browser (I will use [Forsaken](https://www.roblox.com/games/18687417158/Slasher-Forsaken) for this)
2. Open up developer console (F12)
3. Go to Elements tab
4. Search(ctrl + F) for 'universeId=' 
5. Copy the number after the 'universeId=' and input in your programm (in my case 6331902150)
6. give this repo a star

# TODO list (or what i want to get assistance on)
1. Add a mode that will rather count the updates of CCU rather than minutes passed
2. i dunno yet lmao