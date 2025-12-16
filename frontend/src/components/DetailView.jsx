import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { api } from '../api/client';
import { ArrowLeft, CheckCircle, XCircle, AlertTriangle } from 'lucide-react';

export default function DetailView() {
  const { id } = useParams();
  const navigate = useNavigate();
  const [item, setItem] = useState(null);
  const [loading, setLoading] = useState(true);
  const [reason, setReason] = useState('');

  useEffect(() => {
    loadItem();
  }, [id]);

  const loadItem = async () => {
    try {
      const data = await api.getItem(id);
      setItem(data);
    } catch (err) {
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleAction = async (action) => {
    if (!reason) {
      alert('Please provide a reason for this action');
      return;
    }
    try {
      await api.submitAction(id, action, reason);
      loadItem();
      setReason('');
    } catch (err) {
      console.error(err);
    }
  };

  if (loading) return <div className="p-12 text-center text-slate-500">Loading...</div>;
  if (!item) return <div className="p-12 text-center text-slate-500">Item not found</div>;

  return (
    <div className="max-w-4xl mx-auto space-y-6">
      <button onClick={() => navigate('/')} className="flex items-center gap-2 text-slate-500 hover:text-slate-900">
        <ArrowLeft size={16} /> Back to Dashboard
      </button>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {/* Main Content */}
        <div className="md:col-span-2 space-y-6">
          <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
            <div className="flex justify-between items-start mb-4">
              <div>
                <h1 className="text-lg font-semibold text-slate-900">Content Analysis</h1>
                <div className="text-sm text-slate-500">ID: {item.id} • r/{item.subreddit}</div>
              </div>
              <StatusBadge status={item.decision.auto_action} />
            </div>

            <div className="prose prose-sm max-w-none bg-slate-50 p-4 rounded-md border border-slate-100">
              {item.text ? (
                <p className="whitespace-pre-wrap">{item.text}</p>
              ) : (
                <div className="text-slate-400 italic">Image content not displayed in demo</div>
              )}
            </div>
          </div>

          <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
            <h2 className="text-sm font-medium text-slate-900 mb-4">Moderation Signals</h2>
            <div className="grid grid-cols-2 gap-4">
              <SignalCard label="AI Probability" value={item.ai_detection.ai_score} threshold={0.5} />
              <SignalCard label="Hate Speech" value={item.moderation.labels.hate} threshold={0.1} />
              <SignalCard label="Sexual Content" value={item.moderation.labels.sexual} threshold={0.1} />
              <SignalCard label="Violence" value={item.moderation.labels.violence} threshold={0.1} />
            </div>
          </div>
        </div>

        {/* Sidebar Actions */}
        <div className="space-y-6">
          <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
            <h2 className="text-sm font-medium text-slate-900 mb-4">Manual Override</h2>
            <div className="space-y-4">
              <textarea
                className="w-full border rounded-md p-3 text-sm h-24 resize-none"
                placeholder="Reason for action..."
                value={reason}
                onChange={(e) => setReason(e.target.value)}
              />
              <div className="grid grid-cols-2 gap-2">
                <button
                  onClick={() => handleAction('allow')}
                  className="flex items-center justify-center gap-2 bg-green-50 text-green-700 border border-green-200 hover:bg-green-100 py-2 rounded-md text-sm font-medium"
                >
                  <CheckCircle size={16} /> Allow
                </button>
                <button
                  onClick={() => handleAction('block')}
                  className="flex items-center justify-center gap-2 bg-red-50 text-red-700 border border-red-200 hover:bg-red-100 py-2 rounded-md text-sm font-medium"
                >
                  <XCircle size={16} /> Block
                </button>
              </div>
            </div>
          </div>

          <div className="bg-white p-6 rounded-lg border border-slate-200 shadow-sm">
            <h2 className="text-sm font-medium text-slate-900 mb-4">Metadata</h2>
            <dl className="space-y-2 text-sm">
              <div className="flex justify-between">
                <dt className="text-slate-500">Author</dt>
                <dd className="font-mono">{item.author || 'Unknown'}</dd>
              </div>
              <div className="flex justify-between">
                <dt className="text-slate-500">Timestamp</dt>
                <dd>{new Date(item.timestamp).toLocaleString()}</dd>
              </div>
              <div className="flex justify-between">
                <dt className="text-slate-500">Rule ID</dt>
                <dd className="font-mono">{item.decision.rule_id}</dd>
              </div>
            </dl>
          </div>
        </div>
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
    <span className={`px-3 py-1 rounded-full text-sm font-medium uppercase tracking-wide ${styles[status] || 'bg-slate-100 text-slate-600'}`}>
      {status}
    </span>
  );
}

function SignalCard({ label, value, threshold }) {
  const isHigh = value > threshold;
  return (
    <div className={`p-3 rounded-md border ${isHigh ? 'bg-red-50 border-red-100' : 'bg-slate-50 border-slate-100'}`}>
      <div className="text-xs text-slate-500 mb-1">{label}</div>
      <div className={`text-lg font-bold ${isHigh ? 'text-red-700' : 'text-slate-700'}`}>
        {(value * 100).toFixed(1)}%
      </div>
    </div>
  );
}
