import { Engine } from './core/Engine.js';
import { IntroScene } from './scenes/IntroScene.js';
import { MenuScene } from './scenes/MenuScene.js';
import { GameScene } from './scenes/GameScene.js';

// Entry point — bootstraps the engine and manages scene flow
async function main() {
  const loadingBar = document.getElementById('loadingBar');
  const loadingText = document.getElementById('loadingText');
  const loadingScreen = document.getElementById('loadingScreen');

  const setLoading = (pct, msg) => {
    if (loadingBar) loadingBar.style.width = `${pct}%`;
    if (loadingText) loadingText.textContent = msg;
  };

  setLoading(10, 'Awakening the ancient realm...');

  // Init engine
  const engine = new Engine();
  setLoading(25, 'Weaving magical energies...');

  await delay(200);
  setLoading(45, 'Planting the Emerald Forest...');

  await delay(200);
  setLoading(60, 'Summoning ancient creatures...');

  await delay(150);
  setLoading(75, 'Inscribing the sacred quests...');

  await delay(150);
  setLoading(90, 'Opening the gateway...');

  await delay(200);
  setLoading(100, 'The realm awaits...');

  await delay(600);

  // Hide loading screen
  loadingScreen?.classList.add('fade-out');
  await delay(1000);
  loadingScreen?.style && (loadingScreen.style.display = 'none');

  // --- SCENE FLOW ---
  const startGame = (saveData) => {
    const gameScene = new GameScene(engine, saveData || null);
    engine.switchScene(gameScene);
    engine.audio.resume();
    engine.audio.playAmbientMusic();
  };

  const showMenu = () => {
    const menuScene = new MenuScene(engine, {
      onNewGame: () => {
        engine.audio.stopMusic();
        startGame(null);
      },
      onContinue: (save) => {
        engine.audio.stopMusic();
        startGame(save);
      }
    });
    engine.switchScene(menuScene);
  };

  // Start with intro, then menu
  const introScene = new IntroScene(engine, () => {
    showMenu();
  });

  engine.start();
  engine.switchScene(introScene);
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

main().catch(console.error);
