<template>
  <div id="neko-lite" @mousemove="onMouseMove">
    <template v-if="!$client.supported">
      <neko-unsupported />
    </template>

    <template v-else>
      <div class="viewport">
        <neko-video ref="video" :hideControls="true" :extraControls="false" />

        <transition name="toolbar-fade">
          <div v-show="toolbarVisible && connected" class="toolbar" :class="{ hidden: !toolbarShown }" @mousemove.stop>
            <button
              class="btn"
              :class="{ active: hosting || controlling }"
              :disabled="controlLocked"
              title="Control"
              @click.stop.prevent="toggleControl"
            >
              <i class="fas fa-computer-mouse" />
            </button>

            <button class="btn" :disabled="!hosting" title="Clipboard" @click.stop.prevent="openClipboard">
              <i class="fas fa-clipboard" />
            </button>

            <button class="btn" :disabled="!admin" title="Resolution" @click.stop.prevent="openResolution">
              <i class="fas fa-desktop" />
            </button>

            <button class="btn" title="Pause stream" @click.stop.prevent="pause('manual')">
              <i class="fas fa-pause" />
            </button>

            <div class="dropdown">
              <button class="btn" title="Settings" @click.stop.prevent="toggleSettings">
                <i class="fas fa-gear" />
              </button>
              <transition name="dropdown-fade">
                <div v-if="settingsOpen" class="dropdown-panel" @click.stop>
                  <neko-settings />
                </div>
              </transition>
            </div>

            <button class="btn" title="Fullscreen" @click.stop.prevent="fullscreen">
              <i class="fas fa-expand" />
            </button>
          </div>
        </transition>

        <neko-resolution ref="resolution" v-if="admin" />
        <neko-clipboard ref="clipboard" v-if="hosting" />
      </div>

      <div v-if="paused" class="paused" @click.stop.prevent="resume">
        <div class="paused-window">
          <i class="fas fa-pause" />
          <div class="paused-title">Stream paused</div>
          <div class="paused-text">{{ pauseText }}</div>
          <button type="button" @click.stop.prevent="resume">
            {{ pauseReason === 'takeover' ? 'Take back' : 'Resume' }}
          </button>
        </div>
      </div>

      <neko-connect v-else-if="!connected" />
    </template>
  </div>
</template>

<script lang="ts">
  import { Component, Ref, Vue, Watch } from 'vue-property-decorator'

  import Video from '~/components/video.vue'
  import Connect from '~/components/connect.vue'
  import Unsupported from '~/components/unsupported.vue'
  import Clipboard from '~/components/clipboard.vue'
  import Resolution from '~/components/resolution.vue'
  import Settings from '~/components/settings.vue'

  @Component({
    name: 'neko-lite',
    components: {
      'neko-video': Video,
      'neko-connect': Connect,
      'neko-unsupported': Unsupported,
      'neko-clipboard': Clipboard,
      'neko-resolution': Resolution,
      'neko-settings': Settings,
    },
  })
  export default class extends Vue {
    @Ref('video') readonly video!: Video
    @Ref('clipboard') readonly clipboard!: Clipboard
    @Ref('resolution') readonly resolution!: Resolution

    private toolbarShown = true
    private toolbarTimer?: number
    private settingsOpen = false

    // Deactivate on inactivity: drop the WebRTC session when idle so the
    // server stops capturing/encoding (neko keeps encoding a static screen).
    // why the stream is paused: '' (not paused) | idle | background | max | takeover | manual
    private pauseReason = ''
    private pauseMinutes = 0
    private lastActivity = Date.now()
    private hiddenSince = 0
    private viewStart = 0
    private idleTimer?: number
    private readonly activityEvents = ['mousemove', 'mousedown', 'keydown', 'wheel', 'touchstart', 'pointerdown']

    get connected() {
      return this.$accessor.connected
    }

    get admin() {
      return this.$accessor.user.admin
    }

    get hosting() {
      return this.$accessor.remote.hosting
    }

    get controlling() {
      return this.$accessor.remote.controlling
    }

    get controlLocked() {
      return 'control' in this.$accessor.locked && this.$accessor.locked['control'] && !this.$accessor.user.admin
    }

    get paused() {
      return this.pauseReason !== ''
    }

    get pauseText() {
      switch (this.pauseReason) {
        case 'idle':
          return `Paused after ${this.pauseMinutes} min without input.`
        case 'background':
          return 'Paused while the tab was in the background.'
        case 'max':
          return `Paused after ${this.pauseMinutes} min of viewing (max viewing time).`
        case 'takeover':
          return 'Another viewer connected. Only one stream is active at a time.'
        default:
          return 'The stream is paused; the browser keeps running.'
      }
    }

    get lastJoin() {
      return this.$accessor.user.lastJoin
    }

    @Watch('connected')
    onConnectedChanged(connected: boolean) {
      if (connected) this.viewStart = Date.now()
    }

    @Watch('lastJoin')
    onMemberJoined(join: { id: string; at: number }) {
      // single-viewer mode: a newer viewer takes over, this one steps back
      if (!this.$accessor.settings.single_viewer || !this.connected || this.paused) return
      if (!join.id || join.id === this.$client.id) return
      // only viewers that join after our own stream came up (initial members arrive via member/list,
      // our own session is excluded above and cannot match before we are connected)
      if (join.at < this.viewStart) return
      this.pause('takeover')
    }

    get toolbarVisible() {
      // still render the toolbar container only after connection
      return true
    }

    mounted() {
      this.resetToolbarTimer()
      window.addEventListener('keydown', this.onKeyDown)
      document.addEventListener('click', this.onGlobalClick)

      // capture phase: the video element stops propagation of its input events
      for (const ev of this.activityEvents) {
        window.addEventListener(ev, this.onActivity, { capture: true, passive: true })
      }
      document.addEventListener('visibilitychange', this.onVisibilityChange)
      this.idleTimer = window.setInterval(this.checkIdle, 2000)
    }

    beforeDestroy() {
      window.removeEventListener('keydown', this.onKeyDown)
      document.removeEventListener('click', this.onGlobalClick)
      for (const ev of this.activityEvents) {
        window.removeEventListener(ev, this.onActivity, { capture: true })
      }
      document.removeEventListener('visibilitychange', this.onVisibilityChange)
      if (this.idleTimer) {
        clearInterval(this.idleTimer)
        this.idleTimer = undefined
      }
      if (this.toolbarTimer) {
        clearTimeout(this.toolbarTimer)
        this.toolbarTimer = undefined
      }
    }

    onMouseMove() {
      this.toolbarShown = true
      this.resetToolbarTimer()
    }

    resetToolbarTimer() {
      if (this.toolbarTimer) {
        clearTimeout(this.toolbarTimer)
      }
      this.toolbarTimer = window.setTimeout(() => {
        this.toolbarShown = false
        this.settingsOpen = false
      }, 3000)
    }

    onActivity() {
      this.lastActivity = Date.now()
    }

    onVisibilityChange() {
      if (document.visibilityState === 'visible') {
        // coming back to the tab counts as activity
        this.hiddenSince = 0
        this.lastActivity = Date.now()
        // a background pause resumes by itself
        if (this.pauseReason === 'background') this.resume()
        return
      }

      this.hiddenSince = Date.now()
      this.checkIdle()
    }

    checkIdle() {
      const settings = this.$accessor.settings
      if (this.paused || !this.connected) return
      const now = Date.now()

      if (settings.pause_in_background && this.hiddenSince > 0) {
        if (now - this.hiddenSince >= settings.background_minutes * 60 * 1000) {
          this.pause('background')
          return
        }
      }

      if (settings.deactivate_on_inactivity && now - this.lastActivity >= settings.inactivity_minutes * 60 * 1000) {
        this.pauseMinutes = settings.inactivity_minutes
        this.pause('idle')
        return
      }

      if (
        settings.max_view_enabled &&
        this.viewStart > 0 &&
        now - this.viewStart >= settings.max_view_minutes * 60 * 1000
      ) {
        this.pauseMinutes = settings.max_view_minutes
        this.pause('max')
      }
    }

    pause(reason: string) {
      if (this.paused || !this.connected) return
      console.warn(`[neko-lite] stream paused (${reason})`)
      this.pauseReason = reason
      this.settingsOpen = false
      // disconnects websocket + peer but keeps stored credentials
      this.$client.logout()
    }

    resume() {
      if (!this.paused) return
      this.pauseReason = ''
      this.lastActivity = Date.now()
      this.hiddenSince = document.visibilityState === 'visible' ? 0 : Date.now()
      const { displayname, password } = this.$accessor
      if (displayname) {
        this.$accessor.login({ displayname, password })
      }
    }

    onGlobalClick() {
      this.settingsOpen = false
    }

    onKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') {
        this.settingsOpen = false
      }
    }

    toggleControl() {
      this.$accessor.remote.toggle()
    }

    openClipboard() {
      if (!this.hosting) return
      this.clipboard?.open()
    }

    openResolution(e: MouseEvent) {
      if (!this.admin) return
      this.resolution?.open(e)
    }

    toggleSettings() {
      this.settingsOpen = !this.settingsOpen
    }

    fullscreen() {
      this.video?.requestFullscreen()
    }
  }
</script>

<style lang="scss">
  #neko-lite {
    position: absolute;
    inset: 0;
    width: 100vw;
    height: 100vh;
    background: #000;
    overflow: hidden;
    font-family: inherit;
  }

  #neko-lite .viewport {
    position: absolute;
    inset: 0;
  }

  #neko-lite .toolbar {
    position: absolute;
    left: 50%;
    bottom: 18px;
    transform: translateX(-50%);
    display: flex;
    gap: 10px;
    align-items: center;
    padding: 10px 12px;
    border-radius: 999px;
    background: rgba(10, 10, 10, 0.55);
    border: 1px solid rgba(255, 255, 255, 0.08);
    backdrop-filter: blur(10px);
    -webkit-backdrop-filter: blur(10px);
    box-shadow: 0 12px 35px rgba(0, 0, 0, 0.35);
    transition: opacity 180ms ease, transform 180ms ease;
  }

  #neko-lite .toolbar.hidden {
    opacity: 0;
    transform: translateX(-50%) translateY(10px);
    pointer-events: none;
  }

  #neko-lite .toolbar .btn {
    width: 42px;
    height: 42px;
    border-radius: 999px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background: rgba(255, 255, 255, 0.06);
    color: rgba(255, 255, 255, 0.85);
    cursor: pointer;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: background 150ms ease, transform 150ms ease, opacity 150ms ease;
  }

  #neko-lite .toolbar .btn:hover {
    background: rgba(255, 255, 255, 0.12);
    transform: translateY(-1px);
  }

  #neko-lite .toolbar .btn:disabled {
    opacity: 0.35;
    cursor: default;
    transform: none;
  }

  #neko-lite .toolbar .btn.active {
    background: rgba(88, 101, 242, 0.35);
    border-color: rgba(88, 101, 242, 0.45);
  }

  #neko-lite .dropdown {
    position: relative;
  }

  #neko-lite .dropdown-panel {
    position: absolute;
    left: 50%;
    bottom: 56px;
    transform: translateX(-50%);
    min-width: 320px;
    max-width: min(92vw, 400px);
    max-height: 75vh;
    overflow-y: auto;
    background: #161618;
    backdrop-filter: blur(14px);
    -webkit-backdrop-filter: blur(14px);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 12px;
    box-shadow: 0 18px 45px rgba(0, 0, 0, 0.45);
    padding: 10px;
  }

  #neko-lite .paused {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 0, 0, 0.75);
    cursor: pointer;
  }

  #neko-lite .paused-window {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 10px;
    padding: 22px 26px;
    border-radius: 14px;
    background: rgba(20, 20, 20, 0.92);
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 0 22px 60px rgba(0, 0, 0, 0.5);
    color: rgba(255, 255, 255, 0.9);
    text-align: center;
  }

  #neko-lite .paused-window .fa-pause {
    font-size: 26px;
    opacity: 0.8;
  }

  #neko-lite .paused-title {
    font-weight: 700;
    letter-spacing: 0.02em;
  }

  #neko-lite .paused-text {
    font-size: 13px;
    color: rgba(255, 255, 255, 0.65);
  }

  #neko-lite .paused-window button {
    margin-top: 6px;
    padding: 9px 22px;
    border-radius: 10px;
    border: 1px solid rgba(88, 101, 242, 0.45);
    background: rgba(88, 101, 242, 0.85);
    color: #fff;
    font-weight: 600;
    cursor: pointer;
  }

  .toolbar-fade-enter-active,
  .toolbar-fade-leave-active {
    transition: opacity 160ms ease;
  }

  .toolbar-fade-enter,
  .toolbar-fade-leave-to {
    opacity: 0;
  }

  .dropdown-fade-enter-active,
  .dropdown-fade-leave-active {
    transition: opacity 120ms ease, transform 120ms ease;
  }

  .dropdown-fade-enter,
  .dropdown-fade-leave-to {
    opacity: 0;
    transform: translateX(-50%) translateY(6px);
  }
</style>
