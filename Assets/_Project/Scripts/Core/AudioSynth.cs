/*
 * AudioSynth.cs
 * ----------------------------------------------------------------------------
 * All sound is synthesised at runtime — no audio files ship with the game.
 * SFX are rendered once into AudioClips from the SynthDsp recipes; music is a
 * streamed clip whose reader callback mixes the current mood's voice with the
 * previous one while it fades out, like the crossfade in AudioManager.js.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using UnityEngine;

namespace SpiceTrip.Core
{
    public class AudioSynth : MonoBehaviour, ISfx
    {
        AudioSource _music, _sfx;
        readonly Dictionary<string, AudioClip> _clips = new Dictionary<string, AudioClip>();
        readonly List<SynthDsp.MusicVoice> _voices = new List<SynthDsp.MusicVoice>();
        readonly object _lock = new object();
        int _sr;
        int _seed = 1;
        string _mood;
        float _musicVolume = 0.5f, _sfxVolume = 0.7f;
        bool _muted;

        public string CurrentMood => _mood;

        void Awake()
        {
            _sr = AudioSettings.outputSampleRate > 0 ? AudioSettings.outputSampleRate : 48000;
            _music = gameObject.AddComponent<AudioSource>();
            _sfx = gameObject.AddComponent<AudioSource>();
            _music.playOnAwake = false;
            _sfx.playOnAwake = false;
            _music.loop = true;
            _music.clip = AudioClip.Create("music-stream", _sr * 10, 1, _sr, true, OnMusicRead);
            foreach (var name in SynthDsp.SfxNames)
            {
                var data = SynthDsp.RenderSfx(name, _sr);
                var clip = AudioClip.Create("sfx-" + name, data.Length, 1, _sr, false);
                clip.SetData(data, 0);
                _clips[name] = clip;
            }
            ApplyVolumes();
        }

        public void Apply(SettingsData s)
        {
            _musicVolume = s.musicVolume;
            _sfxVolume = s.sfxVolume;
            _muted = s.muted;
            ApplyVolumes();
        }

        void ApplyVolumes()
        {
            float master = _muted ? 0 : 1;
            if (_music) _music.volume = _musicVolume * master;
            if (_sfx) _sfx.volume = _sfxVolume * master;
        }

        public void PlayMusic(string mood)
        {
            if (_mood == mood) return;
            _mood = mood;
            lock (_lock)
            {
                foreach (var v in _voices) v.FadeOut();
                _voices.Add(new SynthDsp.MusicVoice(SynthDsp.MoodFor(mood), _sr, _seed++));
            }
            if (_music && !_music.isPlaying) _music.Play();
        }

        public void Sfx(string name)
        {
            if (_sfx && _clips.TryGetValue(name, out var clip)) _sfx.PlayOneShot(clip);
        }

        void OnMusicRead(float[] data)
        {
            lock (_lock)
            {
                for (int i = 0; i < data.Length; i++)
                {
                    float s = 0;
                    for (int v = 0; v < _voices.Count; v++) s += _voices[v].Next();
                    data[i] = s;
                }
                _voices.RemoveAll(v => v.Finished);
            }
        }
    }
}
