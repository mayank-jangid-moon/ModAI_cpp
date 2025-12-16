const API_BASE = '/api';

export const api = {
  async getStatus() {
    const res = await fetch(`${API_BASE}/status`);
    return res.json();
  },

  async startScraping(subreddit) {
    const res = await fetch(`${API_BASE}/scraper/start`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ subreddit }),
    });
    if (!res.ok) throw new Error('Failed to start scraping');
    return res.json();
  },

  async stopScraping() {
    const res = await fetch(`${API_BASE}/scraper/stop`, {
      method: 'POST',
    });
    return res.json();
  },

  async getItems({ page = 1, limit = 50, status = 'all', search = '' } = {}) {
    const params = new URLSearchParams({ page, limit, status, search });
    const res = await fetch(`${API_BASE}/items?${params}`);
    return res.json();
  },

  async getItem(id) {
    const res = await fetch(`${API_BASE}/items/${id}`);
    if (!res.ok) throw new Error('Item not found');
    return res.json();
  },

  async submitAction(id, action, reason) {
    const res = await fetch(`${API_BASE}/items/${id}/decision`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action, reason }),
    });
    if (!res.ok) throw new Error('Failed to submit action');
    return res.json();
  },

  async getStats() {
    const res = await fetch(`${API_BASE}/stats`);
    return res.json();
  }
};
