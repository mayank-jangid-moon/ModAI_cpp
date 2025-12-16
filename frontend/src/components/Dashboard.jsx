import React, { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import { api } from '../api/client';
import { Play, Square, Search, Filter } from 'lucide-react';

export default function Dashboard() {
  const [status, setStatus] = useState({ scraping_active: false });
  const [stats, setStats] = useState({});
  const [items, setItems] = useState([]);
  const [subreddit, setSubreddit] = useState('');
  const [filter, setFilter] = useState('all');
  const [search, setSearch] = useState('');

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 5000);
    return () => clearInterval(interval);
  }, [filter, search]);

  const loadData = async () => {
    try {
      const [statusData, statsData, itemsData] = await Promise.all([
        api.getStatus(),
        api.getStats(),
        api.getItems({ status: filter, search })
      ]);
      setStatus(statusData);
      setStats(statsData);
      setItems(itemsData.data || []);
    } catch (err) {
      console.error(err);
    }
  };

  const handleStart = async () => {
    if (!subreddit) return;
    await api.startScraping(subreddit);
    loadData();
  };

  const handleStop = async () => {
    await api.stopScraping();
    loadData();
  };

  return (
    <div className="space-y-6">
      {/* Controls & Stats */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
          <h2 className="text-sm font-medium text-slate-500 mb-4">Scraper Control</h2>
          <div className="flex gap-2">
            <input
              type="text"
              placeholder="Subreddit..."
              className="flex-1 px-3 py-2 border rounded-md text-sm"
              value={subreddit}
              onChange={(e) => setSubreddit(e.target.value)}
              disabled={status.scraping_active}
            />
            {!status.scraping_active ? (
              <button onClick={handleStart} className="bg-blue-600 text-white px-4 py-2 rounded-md text-sm font-medium flex items-center gap-2 hover:bg-blue-700">
                <Play size={16} /> Start
              </button>
            ) : (
              <button onClick={handleStop} className="bg-red-600 text-white px-4 py-2 rounded-md text-sm font-medium flex items-center gap-2 hover:bg-red-700">
                <Square size={16} /> Stop
              </button>
            )}
          </div>
          {status.scraping_active && (
            <div className="mt-2 text-xs text-green-600 font-medium flex items-center gap-1">
              <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></span>
              Scraping r/{status.current_subreddit}
            </div>
          )}
        </div>

        <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
          <h2 className="text-sm font-medium text-slate-500 mb-2">Items Scanned</h2>
          <div className="text-3xl font-bold text-slate-900">{stats.total_scanned || 0}</div>
        </div>

        <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
          <h2 className="text-sm font-medium text-slate-500 mb-2">Action Needed</h2>
          <div className="text-3xl font-bold text-amber-600">{stats.action_needed || 0}</div>
        </div>
      </div>

      {/* Filters */}
      <div className="flex gap-4 items-center bg-white p-4 rounded-lg border border-slate-200 shadow-sm">
        <div className="relative flex-1 max-w-sm">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-slate-400" size={16} />
          <input
            type="text"
            placeholder="Search content..."
            className="pl-9 pr-4 py-2 w-full border rounded-md text-sm"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        </div>
        <div className="flex items-center gap-2">
          <Filter size={16} className="text-slate-400" />
          <select
            className="border rounded-md px-3 py-2 text-sm"
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
          >
            <option value="all">All Statuses</option>
            <option value="blocked">Blocked</option>
            <option value="review">Review</option>
            <option value="allowed">Allowed</option>
          </select>
        </div>
      </div>

      {/* Table */}
      <div className="bg-white rounded-lg border border-slate-200 shadow-sm overflow-hidden">
        <table className="w-full text-sm text-left">
          <thead className="bg-slate-50 border-b border-slate-200 text-slate-500 font-medium">
            <tr>
              <th className="px-6 py-3">ID</th>
              <th className="px-6 py-3">Subreddit</th>
              <th className="px-6 py-3">Content Preview</th>
              <th className="px-6 py-3">AI Score</th>
              <th className="px-6 py-3">Status</th>
              <th className="px-6 py-3">Action</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-100">
            {items.map((item) => (
              <tr key={item.id} className="hover:bg-slate-50">
                <td className="px-6 py-3 font-mono text-xs text-slate-500">{item.id}</td>
                <td className="px-6 py-3">{item.subreddit}</td>
                <td className="px-6 py-3 max-w-xs truncate text-slate-600">
                  {item.text || (item.image_path ? '[Image]' : '[No Content]')}
                </td>
                <td className="px-6 py-3">
                  <span className={`px-2 py-1 rounded-full text-xs font-medium ${
                    item.ai_detection.label === 'ai_generated' 
                      ? 'bg-purple-100 text-purple-700' 
                      : 'bg-slate-100 text-slate-600'
                  }`}>
                    {Math.round(item.ai_detection.ai_score * 100)}%
                  </span>
                </td>
                <td className="px-6 py-3">
                  <StatusBadge status={item.decision.auto_action} />
                </td>
                <td className="px-6 py-3">
                  <Link to={`/item/${item.id}`} className="text-blue-600 hover:underline font-medium">
                    View
                  </Link>
                </td>
              </tr>
            ))}
            {items.length === 0 && (
              <tr>
                <td colSpan="6" className="px-6 py-12 text-center text-slate-400">
                  No items found
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function StatusBadge({ status }) {
  const styles = {
    allow: 'bg-green-100 text-green-700',
    block: 'bg-red-100 text-red-700',
    review: 'bg-amber-100 text-amber-700',
  };
  return (
    <span className={`px-2 py-1 rounded-full text-xs font-medium uppercase tracking-wide ${styles[status] || 'bg-slate-100 text-slate-600'}`}>
      {status}
    </span>
  );
}
