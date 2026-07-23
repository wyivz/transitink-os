// Modified by TransitInk OS, 2026: English dialog copy and
// same-origin imports. Based on ESP Web Tools 10.2.1 (Apache-2.0).
// Upstream: https://github.com/esphome/esp-web-tools

import {
  y as baseStyles,
  a as css,
  _ as decorate,
  t as customElement,
  i as LitElement,
  x as html,
} from "./vendor/styles-sT2V1cOw.js";

let NoPortPickedDialog = class extends LitElement {
  render() {
    return html`
      <ew-dialog open @closed=${this._handleClose}>
        <div slot="headline">No port selected</div>
        <div slot="content">
          <div>If the device is missing, check the following:</div>
          <ol>
            <li>Confirm the device is connected to the computer showing this page.</li>
            <li>Confirm the device is powered on; if it has a power LED, it should stay lit.</li>
            <li>Confirm the USB cable supports data, not charge-only.</li>
          </ol>
        </div>
        <div slot="actions">
          ${this.doTryAgain
            ? html`
                <ew-text-button @click=${this.close}>Cancel</ew-text-button>
                <ew-text-button @click=${this.tryAgain}>Try again</ew-text-button>
              `
            : html`<ew-text-button @click=${this.close}>Close</ew-text-button>`}
        </div>
      </ew-dialog>
    `;
  }

  tryAgain() {
    this.close();
    this.doTryAgain?.();
  }

  close() {
    this.shadowRoot.querySelector("ew-dialog").close();
  }

  async _handleClose() {
    this.parentNode.removeChild(this);
  }
};

NoPortPickedDialog.styles = [
  baseStyles,
  css`
    li + li {
      margin-top: 8px;
    }

    ol {
      margin-bottom: 0;
      padding-left: 1.5em;
    }
  `,
];

NoPortPickedDialog = decorate(
  [customElement("ewt-no-port-picked-dialog")],
  NoPortPickedDialog,
);

const openNoPortPickedDialog = async (doTryAgain) => {
  const dialog = document.createElement("ewt-no-port-picked-dialog");
  dialog.doTryAgain = doTryAgain;
  document.body.append(dialog);
  return true;
};

export { openNoPortPickedDialog };
