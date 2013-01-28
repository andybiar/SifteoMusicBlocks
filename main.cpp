/*
 * Sifteo SDK Example.
 */

#define CUBE_ALLOCATION 8

#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

static Metadata M = Metadata()
    .title("TinySynth")
    .package("com.sifteo.sdk.synth", "1.0")
    .cubeRange(1, CUBE_ALLOCATION);

static int16_t sineWave[64];
static const AssetAudio sineAsset = AssetAudio::fromPCM(sineWave);

static int pc = 0;
static int currentBeat = 0;

static bool extraTouch = false;

static VideoBuffer vid[CUBE_ALLOCATION];
static TiltShakeRecognizer motion[CUBE_ALLOCATION];

struct musicCube {
	CubeID id;
	bool play;
	float pitch;
	int beat;
	bool sustain;
	int neighbors;
	bool plus;
	bool sideOffset;
};

static musicCube cubeList[8];

void draw() {
	for (int i = 0; i < CUBE_ALLOCATION; i++) {
		if (cubeList[i].play == false) {
			vid[i].bg0.image(vec(0,0), Offsquare);
			if (cubeList[i].plus) vid[i].bg1.image(vec(4,4), Plus);
			else vid[i].bg1.image(vec(4,4), Minus);
		}
		else {
			vid[i].bg0.image(vec(0,0), Background);
			if (cubeList[i].sustain) {
				vid[i].bg1.image(vec(4,4), PlaySustain);
			}
			else {
				if (cubeList[i].beat == currentBeat) vid[i].bg1.image(vec(4,4), Play);
				else vid[i].bg1.image(vec(4,4), Wait);
			}
		}
	}
}

class SensorListener {
public:
    struct Counter {
        unsigned touch;
        unsigned neighborAdd;
        unsigned neighborRemove;
    } counters[CUBE_ALLOCATION];

    void install()
    {
        Events::neighborAdd.set(&SensorListener::onNeighborAdd, this);
        Events::neighborRemove.set(&SensorListener::onNeighborRemove, this);
        Events::cubeTouch.set(&SensorListener::onTouch, this);
        Events::cubeConnect.set(&SensorListener::onConnect, this);

        // Handle already-connected cubes
        for (CubeID cube : CubeSet::connected())
            onConnect(cube);
    }

private:
    void onConnect(unsigned id)
    {
        CubeID cube(id);
        uint64_t hwid = cube.hwID();

        bzero(counters[id]);
        LOG("Cube %d connected\n", id);

        vid[id].initMode(BG0_SPR_BG1);
        vid[id].attach(id);
        motion[id].attach(id);

		musicCube mc;
		mc.id = cube;
		mc.sustain = false;
		mc.neighbors = 0;
		mc.plus = true;
		mc.sideOffset = 0;

		if (cube == 0) {
			mc.play = true;
			mc.pitch = 392.f;
			mc.beat = 0;
		}
		else {
			mc.play = false;
			mc.pitch = 0;
			mc.beat = -1;
		}
		cubeList[id] = mc;

		AudioChannel(id).play(sineAsset);
		AudioChannel(id).setVolume(0);

		vid[id].bg0.image(vec(0,0), Background);
		vid[id].bg1.setMask(BG1Mask::filled(vec(4,4), vec(8,8)));
    }

    void onTouch(unsigned id)
    {
		if (!cubeList[id].play && !extraTouch) {
			cubeList[id].plus = !cubeList[id].plus;
		}
		else if (!extraTouch) {
			cubeList[id].sustain = !cubeList[id].sustain;
		}
		if (cubeList[id].beat != currentBeat) AudioChannel(id).setVolume(0);
		extraTouch = !extraTouch;
    }

	void resetCube(unsigned id) {
		cubeList[id].play = false;
		cubeList[id].sideOffset = 0;
	}

    void onNeighborRemove(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
		cubeList[firstID].neighbors--;
		cubeList[secondID].neighbors--;

		if (cubeList[firstID].neighbors == 0 && firstID != 0) resetCube(firstID);
		if (cubeList[secondID].neighbors == 0 && secondID != 0) resetCube(secondID);
    }

	// first ID = the neighbor connected to
	// second ID = the one that moved over
    void onNeighborAdd(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
		cubeList[firstID].neighbors++;
		cubeList[secondID].neighbors++;

        if (cubeList[secondID].play == true) {
			updateNeighbors(secondID, secondSide, firstID, firstSide);
		}
		else if (cubeList[firstID].play == true) {
			updateNeighbors(firstID, firstSide, secondID, secondSide);
		}
    }

	void updateNeighbors(unsigned old, unsigned oldside, unsigned baby, unsigned babyside) {
		musicCube *babe = &cubeList[baby];
		musicCube *mom = &cubeList[old];

		float pitchFactor;

		if (babyside == 0) pitchFactor = (9.f / 8);
		else if (babyside == 1) pitchFactor = (4.f / 3);
		else if (babyside == 2) pitchFactor = (81.f / 64);
		else pitchFactor = (32.f / 27);

		if (babe->plus) babe->pitch = mom->pitch * pitchFactor;
		else babe->pitch = mom->pitch / pitchFactor;

		babe->play = true;

		if (oldside % 2 == 0) {
			if (babyside % 2 != 0) babe->sideOffset = 1;
		}
		else if (babyside % 2 == 0) babe->sideOffset = 1;
		
		if (oldside % 2 + mom->sideOffset == 0) babe->beat = mom->beat;
		else babe->beat = (mom->beat + 1) % 4;
	}
};

void synthInit()
{
    for (int i = 0; i != arraysize(sineWave); i++) {
        float theta = i * float(M_PI * 2 / arraysize(sineWave));
        sineWave[i] = sin(theta) * 0x7fff;
    }
}

void synthesize()
{
	// UPDATE TIMING
	if (pc % 33 == 0) {
		for (int i = 0; i < CUBE_ALLOCATION; i++) {
			if ((cubeList[i].beat == currentBeat) && (cubeList[i].sustain == false)) {
				AudioChannel(i).setVolume(0);
			}
		}
		currentBeat++;
		currentBeat = currentBeat % 4;
		pc = 0;
	}

	pc++;

	// MAKE SOME SOUND
	for (int i = 0; i < CUBE_ALLOCATION; i++) {
		if (cubeList[i].play && (cubeList[i].beat == currentBeat || cubeList[i].sustain)) {
			AudioChannel(i).setSpeed(cubeList[i].pitch * arraysize(sineWave));
			AudioChannel(i).setVolume(48);
		}
	}
}

void main()
{
	synthInit();

	for (int i = 0; i < CUBE_ALLOCATION; i++) cubeList[i].play = false;

	static SensorListener sensors;
    sensors.install();

    while (1) {
        synthesize();
		draw();
        System::paint();
    }
}