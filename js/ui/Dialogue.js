// Dialogue system — displays NPC conversation UI
export class DialogueSystem {
  constructor(questSystem, inventory, audio) {
    this.questSystem = questSystem;
    this.inventory = inventory;
    this.audio = audio;

    this.isOpen = false;
    this.currentNPC = null;

    this._el = {
      box: document.getElementById('dialogueBox'),
      portrait: document.getElementById('dialoguePortrait'),
      name: document.getElementById('dialogueName'),
      text: document.getElementById('dialogueText'),
      options: document.getElementById('dialogueOptions')
    };

    this._typewriterTimeout = null;
  }

  open(npc) {
    if (this.isOpen) return;
    this.currentNPC = npc;
    this.isOpen = true;

    this.audio?.playDialogueOpen();

    const dialogueDef = npc.def.dialogues;

    // Determine which dialogue node to use based on quest state
    let nodeKey = 'greeting';
    const questId = npc.def.questId;
    if (questId) {
      if (this.questSystem.isComplete(questId)) nodeKey = 'quest_complete';
      else if (this.questSystem.isActive(questId)) nodeKey = 'quest_active';
      else nodeKey = 'greeting';
    }

    this._showNode(npc, nodeKey);

    if (this._el.box) this._el.box.classList.remove('hidden');

    // Report talking to quest system
    this.questSystem.reportTalk(npc.id);
  }

  _showNode(npc, nodeKey) {
    const node = npc.getDialogueNode(nodeKey);
    if (!node) { this.close(); return; }

    if (this._el.portrait) this._el.portrait.textContent = npc.def.icon;
    if (this._el.name) this._el.name.textContent = npc.def.name;

    // Typewriter effect
    this._typewriteText(node.text, () => {
      this._showOptions(npc, node);
    });
  }

  _typewriteText(text, onComplete) {
    if (!this._el.text) return onComplete?.();
    this._el.text.textContent = '';
    if (this._typewriterTimeout) clearTimeout(this._typewriterTimeout);

    let idx = 0;
    const speed = 22; // ms per character

    const type = () => {
      if (idx < text.length) {
        this._el.text.textContent += text[idx++];
        this._typewriterTimeout = setTimeout(type, speed);
      } else {
        onComplete?.();
      }
    };

    // Allow clicking to skip typewriter
    const skip = () => {
      if (idx < text.length) {
        if (this._typewriterTimeout) clearTimeout(this._typewriterTimeout);
        this._el.text.textContent = text;
        idx = text.length;
        onComplete?.();
      }
    };

    this._el.text.onclick = skip;
    type();
  }

  _showOptions(npc, node) {
    if (!this._el.options) return;
    this._el.options.innerHTML = '';

    if (!node.options?.length) {
      this._addCloseOption();
      return;
    }

    node.options.forEach(opt => {
      if (!opt) return;
      const btn = document.createElement('button');
      btn.className = 'dialogue-option';
      btn.textContent = opt.text;
      btn.addEventListener('click', () => {
        // Handle action
        if (opt.action) this._handleAction(opt.action);

        if (opt.next) {
          this._showNode(npc, opt.next);
        } else {
          this.close();
        }
      });
      this._el.options.appendChild(btn);
    });
  }

  _handleAction(action) {
    if (action.startsWith('start_quest:')) {
      const questId = action.split(':')[1];
      this.questSystem.startQuest(questId);
    } else if (action === 'open_shop') {
      this._openShop(this.currentNPC);
    }
  }

  _openShop(npc) {
    // Simple shop via toast notifications (full shop UI would need more space)
    if (!npc.def.shop) return;
    npc.def.shop.forEach(item => {
      console.log(`Shop item: ${item.name} — ${item.cost} gold`);
    });
    // For now show a notice
    const hud = document.getElementById('toastContainer');
    const t = document.createElement('div');
    t.className = 'toast item';
    t.textContent = '🛍 Shop: Check console (full UI in progress)';
    hud?.appendChild(t);
    setTimeout(() => t.remove(), 3000);
  }

  _addCloseOption() {
    const btn = document.createElement('button');
    btn.className = 'dialogue-option';
    btn.textContent = '[ Farewell ]';
    btn.addEventListener('click', () => this.close());
    if (this._el.options) this._el.options.appendChild(btn);
  }

  close() {
    if (!this.isOpen) return;
    this.isOpen = false;
    this.currentNPC = null;
    if (this._el.box) this._el.box.classList.add('hidden');
    if (this._typewriterTimeout) clearTimeout(this._typewriterTimeout);
    if (this._el.text) this._el.text.onclick = null;
  }
}
