#pragma once

namespace ElysiaModel
{
    struct LoadedModel;

    namespace MaterialOverrides
    {
        // Restores Saved parameter edits. With no file, base color tint becomes white.
        void Apply(LoadedModel& model);
        void SaveIfDirty(LoadedModel& model);
    }
}
