/** Navigation policy; the native adapter must execute all UI calls on Nickel's GUI thread.
 * Return contexts are opaque, session-only adapter handles, never persisted positions.
 * Adapter promises resolve only when the requested view transition is confirmed.
 */
export class BookNotebookNavigation {
  constructor({ links, native }) {
    this.links = links;
    this.native = native;
    this.session = null;
    this.busy = false;
  }

  async exclusive(action) {
    if (this.busy) throw new Error('Navigation already in progress');
    this.busy = true;
    try { return await action(); }
    finally { this.busy = false; }
  }

  async openLinkedNotebook() {
    return this.exclusive(async () => {
      if (this.session) throw new Error('Return to the book before opening another linked notebook');
      const reader = await this.native.captureReader();
      if (!reader || typeof reader.bookId !== 'string' || !reader.bookId.trim()
          || reader.context == null) throw new Error('No active book');
      // Every acquired reader handle is either retained by this session or released.
      let retained = false;
      try {
        const notebookId = await this.links.get(reader.bookId);
        if (!notebookId) throw new Error('No notebook linked to this book');
        const notebook = await this.native.resolveNotebook(notebookId);
        if (!notebook || notebook.kind !== 'basic') throw new Error('Linked basic notebook is unavailable');
        // The adapter uses the native resource-checking opener. On rejection it
        // must leave/restore the reader; an unconfirmed transition is not success.
        await this.native.openNotebook(notebook, reader.context);
        this.session = { reader, notebookId };
        retained = true;
        return { bookId: reader.bookId, notebookId };
      } finally {
        if (!retained) await this.native.releaseReader(reader.context);
      }
    });
  }

  async returnToBook() {
    return this.exclusive(async () => {
      if (!this.session) throw new Error('No linked book to return to');
      const { reader, notebookId } = this.session;
      // Native save/close owns handwriting persistence. Cancellation keeps the
      // session intact; no file copying or database writes can substitute for it.
      if (!await this.native.prepareNotebookExit(notebookId)) return false;
      await this.native.restoreReader(reader.context);
      this.session = null;
      await this.native.releaseReader(reader.context);
      return true;
    });
  }
}
