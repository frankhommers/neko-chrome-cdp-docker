<template>
  <div class="connect">
    <div class="window">
      <template v-if="autoConnecting || connecting">
        <div class="title">Connecting…</div>
        <div class="loader">
          <div class="bounce1"></div>
          <div class="bounce2"></div>
        </div>
      </template>

      <form v-else class="form" @submit.stop.prevent="submit">
        <div class="title">Connect</div>
        <label>
          <span>Display name</span>
          <input type="text" autocomplete="name" v-model.trim="displayname" autofocus />
        </label>
        <label>
          <span>Password</span>
          <input type="password" autocomplete="current-password" v-model="password" />
        </label>
        <button type="submit">Connect</button>
      </form>
    </div>
  </div>
</template>

<script lang="ts">
  import { Component, Vue } from 'vue-property-decorator'

  @Component({ name: 'neko-connect' })
  export default class extends Vue {
    private displayname: string = ''
    private password: string = ''
    private autoConnecting = false

    mounted() {
      const params = new URL(location.href).searchParams

      const qPassword = params.get('password')
      const qName = params.get('name')

      // ?scroll=1..100 sets scroll sensitivity and persists it (upstream #681)
      const qScroll = parseInt(params.get('scroll') || '', 10)
      if (!isNaN(qScroll)) {
        this.$accessor.settings.setScroll(Math.max(1, Math.min(qScroll, 100)))
      }

      // prefill from local storage-backed store
      this.displayname = this.$accessor.displayname || ''
      this.password = this.$accessor.password || ''

      if (qName) this.displayname = qName
      if (qPassword) this.password = qPassword

      if (qName && qPassword) {
        this.autoConnecting = true
        this.$nextTick(() => {
          this.$accessor.login({ displayname: this.displayname, password: this.password })
        })
      }
    }

    get connecting() {
      return this.$accessor.connecting
    }

    submit() {
      if (!this.displayname) {
        window.alert('Display name is required')
        return
      }
      this.$accessor.login({ displayname: this.displayname, password: this.password })
    }
  }
</script>

<style lang="scss" scoped>
  .connect {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.75);
    display: flex;
    justify-content: center;
    align-items: center;
  }

  .window {
    width: min(420px, 92vw);
    background: rgba(20, 20, 20, 0.92);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 14px;
    padding: 18px;
    box-shadow: 0 22px 60px rgba(0, 0, 0, 0.5);
  }

  .title {
    color: rgba(255, 255, 255, 0.92);
    font-weight: 700;
    letter-spacing: 0.02em;
    margin-bottom: 12px;
    text-align: center;
  }

  .form {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }

  label {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  label span {
    font-size: 12px;
    color: rgba(255, 255, 255, 0.7);
  }

  input {
    border: 1px solid rgba(255, 255, 255, 0.1);
    padding: 10px 12px;
    border-radius: 10px;
    background: rgba(255, 255, 255, 0.06);
    color: rgba(255, 255, 255, 0.9);
    outline: none;
  }

  button {
    margin-top: 6px;
    cursor: pointer;
    border-radius: 10px;
    padding: 10px 12px;
    background: rgba(88, 101, 242, 0.9);
    border: 1px solid rgba(88, 101, 242, 1);
    color: #fff;
    font-weight: 700;
  }

  button:hover {
    filter: brightness(1.05);
  }

  .loader {
    width: 90px;
    height: 90px;
    position: relative;
    margin: 0 auto;
  }

  .bounce1,
  .bounce2 {
    width: 100%;
    height: 100%;
    border-radius: 50%;
    background-color: rgba(88, 101, 242, 0.95);
    opacity: 0.6;
    position: absolute;
    top: 0;
    left: 0;
    animation: bounce 2s infinite ease-in-out;
  }

  .bounce2 {
    animation-delay: -1s;
  }

  @keyframes bounce {
    0%,
    100% {
      transform: scale(0);
    }
    50% {
      transform: scale(1);
    }
  }
</style>
