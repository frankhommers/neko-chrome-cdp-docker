<template>
  <div class="settings">
    <div class="section">Viewing</div>

    <div class="row">
      <span title="When another viewer connects, this stream pauses. Only one stream is encoded at a time.">
        One stream at a time
      </span>
      <label class="switch">
        <input type="checkbox" v-model="single_viewer" />
        <span />
      </label>
    </div>

    <div class="row">
      <span title="Pause the stream after a period without mouse/keyboard input">Pause when idle</span>
      <div class="controls">
        <label class="select" v-if="deactivate_on_inactivity">
          <select v-model.number="inactivity_minutes">
            <option v-for="m in inactivity_options" :key="m" :value="m">{{ m }} min</option>
          </select>
        </label>
        <label class="switch">
          <input type="checkbox" v-model="deactivate_on_inactivity" />
          <span />
        </label>
      </div>
    </div>

    <div class="row">
      <span title="Pause while this tab is in the background; resumes automatically when you come back">
        Pause in background
      </span>
      <div class="controls">
        <label class="select" v-if="pause_in_background">
          <select v-model.number="background_minutes">
            <option v-for="m in background_options" :key="m" :value="m">{{ m === 0 ? 'direct' : m + ' min' }}</option>
          </select>
        </label>
        <label class="switch">
          <input type="checkbox" v-model="pause_in_background" />
          <span />
        </label>
      </div>
    </div>

    <div class="row">
      <span title="Pause after this much viewing time, even while active">Max viewing time</span>
      <div class="controls">
        <label class="select" v-if="max_view_enabled">
          <select v-model.number="max_view_minutes">
            <option v-for="m in max_view_options" :key="m" :value="m">{{ m < 60 ? m + ' min' : m / 60 + ' h' }}</option>
          </select>
        </label>
        <label class="switch">
          <input type="checkbox" v-model="max_view_enabled" />
          <span />
        </label>
      </div>
    </div>

    <div class="section">Input</div>

    <div class="row">
      <span>Scroll sensitivity</span>
      <label class="slider">
        <input type="range" min="1" max="100" v-model="scroll" />
        <small>{{ scroll }}</small>
      </label>
    </div>

    <div class="row">
      <span>Invert scroll</span>
      <label class="switch">
        <input type="checkbox" v-model="scroll_invert" />
        <span />
      </label>
    </div>

    <div class="row">
      <span>Autoplay</span>
      <label class="switch">
        <input type="checkbox" v-model="autoplay" />
        <span />
      </label>
    </div>

    <div class="row">
      <span>Keyboard layout</span>
      <label class="select">
        <select v-model="keyboard_layout">
          <option v-for="(name, code) in keyboard_layouts" :key="code" :value="code">{{ name }}</option>
        </select>
      </label>
    </div>
  </div>
</template>

<script lang="ts">
  import { Component, Vue } from 'vue-property-decorator'

  @Component({ name: 'neko-settings' })
  export default class extends Vue {
    get scroll() {
      return this.$accessor.settings.scroll.toString()
    }

    set scroll(value: string) {
      this.$accessor.settings.setScroll(parseInt(value, 10))
    }

    get scroll_invert() {
      return this.$accessor.settings.scroll_invert
    }

    set scroll_invert(value: boolean) {
      this.$accessor.settings.setInvert(value)
    }

    get autoplay() {
      return this.$accessor.settings.autoplay
    }

    set autoplay(value: boolean) {
      this.$accessor.settings.setAutoplay(value)
    }

    get deactivate_on_inactivity() {
      return this.$accessor.settings.deactivate_on_inactivity
    }

    set deactivate_on_inactivity(value: boolean) {
      this.$accessor.settings.setDeactivateOnInactivity(value)
    }

    get inactivity_minutes() {
      return this.$accessor.settings.inactivity_minutes
    }

    set inactivity_minutes(value: number) {
      this.$accessor.settings.setInactivityMinutes(value)
    }

    get inactivity_options() {
      return [1, 2, 5, 10, 15, 30, 60]
    }

    get single_viewer() {
      return this.$accessor.settings.single_viewer
    }

    set single_viewer(value: boolean) {
      this.$accessor.settings.setSingleViewer(value)
    }

    get pause_in_background() {
      return this.$accessor.settings.pause_in_background
    }

    set pause_in_background(value: boolean) {
      this.$accessor.settings.setPauseInBackground(value)
    }

    get background_minutes() {
      return this.$accessor.settings.background_minutes
    }

    set background_minutes(value: number) {
      this.$accessor.settings.setBackgroundMinutes(value)
    }

    get background_options() {
      return [0, 1, 2, 5, 10]
    }

    get max_view_enabled() {
      return this.$accessor.settings.max_view_enabled
    }

    set max_view_enabled(value: boolean) {
      this.$accessor.settings.setMaxViewEnabled(value)
    }

    get max_view_minutes() {
      return this.$accessor.settings.max_view_minutes
    }

    set max_view_minutes(value: number) {
      this.$accessor.settings.setMaxViewMinutes(value)
    }

    get max_view_options() {
      return [15, 30, 60, 120, 240]
    }

    get keyboard_layout() {
      return this.$accessor.settings.keyboard_layout
    }

    set keyboard_layout(value: string) {
      this.$accessor.settings.setKeyboardLayout(value)
      this.$accessor.remote.changeKeyboard()
    }

    get keyboard_layouts() {
      const list = this.$accessor.settings.keyboard_layouts_list || {}

      // Ensure common layouts appear even if the server list isn't loaded yet.
      const common: Record<string, string> = {
        us: 'English (US)',
        uk: 'English (UK)',
        de: 'German',
        fr: 'French',
        es: 'Spanish',
        it: 'Italian',
        nl: 'Dutch',
      }

      return { ...common, ...list }
    }
  }
</script>

<style lang="scss" scoped>
  .settings {
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 2px 0 4px;
    color: rgba(255, 255, 255, 0.9);
    user-select: none;
  }

  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 8px 10px;
    border-radius: 10px;
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.06);
  }

  .section {
    margin: 4px 2px -2px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: rgba(255, 255, 255, 0.45);
  }

  .controls {
    display: inline-flex;
    align-items: center;
    gap: 10px;
  }

  .row > span {
    font-size: 13px;
    color: rgba(255, 255, 255, 0.75);
  }

  .slider {
    display: inline-flex;
    align-items: center;
    gap: 10px;
  }

  input[type='range'] {
    width: 150px;
    background: transparent;
    height: 20px;
    -webkit-appearance: none;
  }

  input[type='range']::-webkit-slider-thumb {
    -webkit-appearance: none;
    height: 12px;
    width: 12px;
    border-radius: 12px;
    background: #fff;
    cursor: pointer;
    margin-top: -4px;
  }

  input[type='range']::-webkit-slider-runnable-track {
    width: 100%;
    height: 4px;
    cursor: pointer;
    background: rgba(88, 101, 242, 0.95);
    border-radius: 2px;
  }

  input[type='range']::-moz-range-thumb {
    height: 12px;
    width: 12px;
    border-radius: 12px;
    background: #fff;
    cursor: pointer;
  }

  input[type='range']::-moz-range-track {
    width: 100%;
    height: 4px;
    cursor: pointer;
    background: rgba(88, 101, 242, 0.95);
    border-radius: 2px;
  }

  .switch {
    position: relative;
    width: 42px;
    height: 24px;
  }

  .switch input {
    opacity: 0;
    width: 0;
    height: 0;
  }

  .switch span {
    position: absolute;
    cursor: pointer;
    inset: 0;
    background-color: rgba(255, 255, 255, 0.08);
    transition: 0.2s;
    border-radius: 34px;
  }

  .switch span:before {
    position: absolute;
    content: '';
    height: 18px;
    width: 18px;
    left: 3px;
    bottom: 3px;
    background-color: white;
    transition: 0.3s;
    border-radius: 50%;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
  }

  .switch input:checked + span {
    background-color: rgba(88, 101, 242, 0.9);
  }

  .switch input:checked + span:before {
    transform: translateX(18px);
  }

  .select select {
    appearance: none;
    border-radius: 8px;
    padding: 5px 26px 5px 10px;
    border: 1px solid rgba(255, 255, 255, 0.12);
    background-color: rgba(255, 255, 255, 0.06);
    /* chevron */
    background-image: linear-gradient(45deg, transparent 50%, rgba(255, 255, 255, 0.7) 50%),
      linear-gradient(135deg, rgba(255, 255, 255, 0.7) 50%, transparent 50%);
    background-position: calc(100% - 13px) 50%, calc(100% - 9px) 50%;
    background-size: 4px 4px, 4px 4px;
    background-repeat: no-repeat;
    color: rgba(255, 255, 255, 0.9);
    font: inherit;
    font-size: 12px;
    cursor: pointer;
    max-width: 150px;
  }

  .select select option {
    background: #1c1c1e;
  }

  .row > span {
    white-space: nowrap;
  }

  .switch span {
    border: 1px solid rgba(255, 255, 255, 0.12);
  }
</style>
