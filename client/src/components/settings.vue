<template>
  <div class="settings">
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
      <span
        title="Disconnect the stream after a period without mouse/keyboard input, so the server stops encoding video"
        >Deactivate on inactivity</span
      >
      <label class="switch">
        <input type="checkbox" v-model="deactivate_on_inactivity" />
        <span />
      </label>
    </div>

    <div class="row" v-if="deactivate_on_inactivity">
      <span>Inactivity timeout</span>
      <label class="select">
        <select v-model.number="inactivity_minutes">
          <option v-for="m in inactivity_options" :key="m" :value="m">{{ m }} min</option>
        </select>
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
    gap: 10px;
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
    border-radius: 10px;
    padding: 8px 10px;
    border: 1px solid rgba(255, 255, 255, 0.1);
    background: rgba(255, 255, 255, 0.06);
    color: rgba(255, 255, 255, 0.9);
    cursor: pointer;
  }
</style>
